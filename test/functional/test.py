#!/usr/bin/env python3

import os
import sys
import argparse
import numpy as np
import h5py
import shutil
import subprocess
import gzip
import time
# from scipy.fftpack import fft
# from scipy import stats

from pygments.lexers import JsonLexer
from json import dumps

import jinja2 as j2

import colorama
from colorama import Fore, Back, Style
LOGLEVEL=0

me = os.path.realpath(os.path.dirname(__file__))

regression_dir = os.path.join(os.sep, 'home', 'cosmonaut', 'regression')

sys.path.append(me + '/../../lib/python/')

from picopic.meta_reader import MetaReader
from picopic.plot_builder import PlotBuilder
from picopic.h5_reader import H5Reader
## end import path

class Util ():
    def __init__(self):
        True

    def cliexec (self, cmd, cwd=None, view=False, wait=False):
        if not cwd:
            cwd = me
        p = subprocess.Popen(cmd, cwd=cwd, shell=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
        if view:
            output = p.stdout.readline()
            while True:
                output = p.stdout.readline()
                if output.decode() == '' and p.poll() is not None:
                    break
                if output:
                    print(output.decode().strip())
            p.poll()

        if wait:
            p.wait()
        return p.returncode


    def mkdir (self, path, force=False):
        if not os.path.exists(path):
            os.makedirs(path)
        else:
            if force:
                try:
                    shutil.rmtree(path, ignore_errors=True)
                except:
                    os.remove(path)
                os.makedirs(path)
            else:
                raise OSError("Directory {} already exists".format(path))

    def cp (self, src, dst, recursively=True):
        if os.path.isfile(src):
            _dst = os.path.join(dst, os.path.basename(src))
            shutil.copyfile(src, _dst, follow_symlinks=True)
            shutil.copymode(src, _dst)
        else:
            if os.path.isdir(dst):
                _dst = os.path.join(dst, os.path.basename(src))
            else:
                _dst = dst
            shutil.copytree(src, _dst, symlinks=True, ignore=None,
                            ignore_dangling_symlinks=False)

class bootstrap ():
    def __init__(self, testdir='testdir', parameters_template_name='picopic.json.tmpl',
                 result_path='.', data_root='simulation_result', keep_working_dir=False,
                 accept_ieee=True, verbose=False, debug=False):
        self.rootdir = os.path.realpath(os.path.dirname(os.path.dirname(me)))
        self.testdir = os.path.join(self.rootdir, testdir)
        self.keep_working_dir = keep_working_dir
        self.accept_ieee = accept_ieee
        self.verbose = verbose or debug
        self.debug = debug
        tmpldir = me

        utils = Util()

        print("Preparing the code")
        utils.cliexec('make distclean', cwd=self.rootdir, view=self.verbose, wait=True)
        utils.cliexec('./autogen.sh', cwd=self.rootdir, view=self.verbose, wait=True)
        if self.accept_ieee:
            utils.cliexec('./configure --enable-ieee --disable-singlethread', cwd=self.rootdir, view=self.verbose, wait=True)
        else:
            utils.cliexec('./configure --disable-singlethread', cwd=self.rootdir, view=self.verbose, wait=True)
        utils.cliexec('make build', cwd=self.rootdir, view=self.verbose, wait=True)

        picopic_file = os.path.join(self.rootdir, 'PiCoPiC')
        tools_dir = os.path.join(self.rootdir, 'tools')

        print("Copying files to testing directory")
        print(self.testdir)
        utils.mkdir(self.testdir, True)
        utils.cp(picopic_file, self.testdir)
        utils.cp(tools_dir, self.testdir)

        tmpl_path = os.path.join(tmpldir, parameters_template_name)

        j2_env = j2.Environment(loader=j2.FileSystemLoader(tmpldir), trim_blocks=True)
        rendered_tmpl = j2_env.get_template(parameters_template_name).render(
            result_path=result_path,
            macro_amount=2.1e5
            )

        with open(os.path.join(self.testdir, 'PiCoPiC.json'), "w") as f:
            f.write(rendered_tmpl)

        print("Launching application to prepare data for testing")

        if self.verbose: print("\nApplication Output:\n===================\n")
        start_time = time.time()
        utils.cliexec(os.path.join(self.testdir, 'PiCoPiC'), cwd=self.testdir, view=self.verbose, wait=True)
        end_time = time.time()
        if self.verbose: print("\nEnd of application Output.\n==========================\n")
        ## print time report
        print(Fore.CYAN + "Execution time is", round(end_time - start_time, 2),  "s." + Style.RESET_ALL)


    def __del__(self):
        utils = Util()
        print("Clearing testing data")
        if not self.keep_working_dir:
            shutil.rmtree(self.testdir, ignore_errors=True)
        utils.cliexec('make clean', cwd=self.rootdir, view=False, wait=True)


class picopicTest ():
    def __init__(self, config_file, rel_tolerance=10000, abs_tolerance=0):
        self.meta = MetaReader(config_file)
        self.components = {}
        self.rootdir = os.path.realpath(os.path.dirname(os.path.dirname(me)))
        self.testdir = os.path.join(self.rootdir, os.path.dirname(config_file))
        self.r_tol = rel_tolerance
        self.a_tol = abs_tolerance
        self.verbose = False

        self.true_data_dir = os.path.join(me, 'true_data')

    def fetch_test_data(self, component_name, filename):
        frame_name = self.components[component_name]
        h5file = h5py.File(os.path.join(self.testdir, self.meta.data_path, 'data.h5'), 'r')

        dset = h5file[os.path.join('/', component_name, frame_name)][filename]
        shape = dset.shape
        test_data = np.reshape(dset[...], (shape[0] * shape[1]))

        return(test_data)


    def compare(self, component_name, filename):
        frame_name = self.components[component_name]
        true_path = os.path.join(me, 'true_data', component_name, frame_name, str(filename))

        test_data = self.fetch_test_data(component_name, filename)
        true_data = np.fromfile('{}.dat'.format(true_path), dtype=float, sep=' ')

        isc = True

        # mean
        test_value_mean = np.nanmean(test_data)
        true_value_mean = true_data[0]
        isc = np.allclose(test_value_mean, true_value_mean, rtol=self.r_tol, atol=self.a_tol)

        # standard deviation
        test_value_std = np.nanstd(test_data)
        true_value_std = true_data[1]
        isc = np.allclose(test_value_std, true_value_std, rtol=self.r_tol, atol=self.a_tol)

        # variance
        test_value_var = np.nanvar(test_data)
        true_value_var = true_data[2]
        if isc:
            isc = np.allclose(test_value_var, true_value_var, rtol=self.r_tol, atol=self.a_tol)

        if isc:
            print("Data matching for " + component_name + ":" + str(filename),
                  Fore.BLUE + "PASSED" + Style.RESET_ALL)
        else:
            print("Data matching for " + component_name + ":" + str(filename),
                  Fore.RED + "FAILED" + Style.RESET_ALL)

        if self.verbose:
            print(Fore.YELLOW, "Standard Deviation of",
                  component_name,
                  ". test data:",
                  str(test_value_std),
                  ", true data:",
                  str(true_value_std),
                  "; Ralative Difference:",
                  str(abs((test_value_std - true_value_std) / true_value_std)),
                  Style.RESET_ALL)

            print(Fore.YELLOW, "Variance of",
                  component_name,
                  ". test data:",
                  str(test_value_var),
                  ", true data:",
                  str(true_value_var),
                  "; Ralative Difference:",
                  str(abs((test_value_var - true_value_var) / true_value_var)),
                  Style.RESET_ALL)

        return isc


def test_example(template_name, number, accept_ieee=True,
                 rel_tolerance=10000, verbose=False, debug=False):
    status=True

    b = bootstrap(testdir='testingdir',
                  parameters_template_name=template_name,
                  keep_working_dir=debug, # if debug, keep working dir for analysis
                  accept_ieee=accept_ieee,
                  verbose=verbose,
                  debug=debug)

    t = picopicTest(os.path.join(b.testdir, 'PiCoPiC.json'),
                 rel_tolerance=rel_tolerance,
                 abs_tolerance=0) # use abs tolerance to avoid comparing of very small numbers
    t.verbose = verbose
    t.debug = debug

    t.components['E/r'] = 'rec/0-32_0-128'
    t.components['E/phi'] = 'rec/0-32_0-128'
    t.components['E/z'] = 'rec/0-32_0-128'

    t.components['H/r'] = 'rec/0-32_0-128'
    t.components['H/phi'] = 'rec/0-32_0-128'
    t.components['H/z'] = 'rec/0-32_0-128'

    t.components['J/r'] = 'rec/0-32_0-128'
    t.components['J/phi'] = 'rec/0-32_0-128'
    t.components['J/z'] = 'rec/0-32_0-128'

    # compare positions and velocities
    # t.components['T/Electrons'] = 'frame_100:100_150:150'
    # t.components['T/Ions'] = 'frame_100:100_150:150'

    components = ['E/r', 'E/phi', 'E/z', 'H/r', 'H/phi', 'H/z', 'J/r', 'J/phi', 'J/z']
    for i in components:
        s = t.compare(i, number)
        if status: status = s

    return status


def collect_true_data (template_name, number, accept_ieee=True,
                       verbose=False, debug=False):

    components = ['E/r', 'E/phi', 'E/z', 'H/r', 'H/phi', 'H/z', 'J/r', 'J/phi', 'J/z']

    mean_dic = {}
    std_dic = {}
    var_dic = {}

    mean_dic_mean = {}
    std_dic_mean = {}
    var_dic_mean = {}

    for c in components:
        mean_dic[c] = []
        std_dic[c] = []
        var_dic[c] = []

    b = bootstrap(testdir='testingdir',
                  parameters_template_name=template_name,
                  keep_working_dir=debug, # if debug, keep working dir for analysis
                  accept_ieee=accept_ieee,
                  verbose=verbose,
                  debug=debug)

    for i in range(0, 100000):

        print(Fore.YELLOW + "Launch iteration " + str(i) + Style.RESET_ALL)

        t = picopicTest(os.path.join(b.testdir, 'PiCoPiC.json'),
                        rel_tolerance=0,
                        abs_tolerance=0)
        t.verbose = verbose
        t.debug = debug

        for c in components:
            t.components[c] = 'rec/0-32_0-128'

        # compare positions and velocities
        t.components['T/Electrons'] = 'frame_100:100_150:150'
        t.components['T/Ions'] = 'frame_100:100_150:150'

        for c in components:
            test_data = t.fetch_test_data(c, number)
            test_value_mean = np.nanmean(test_data)
            test_value_std = np.nanstd(test_data)
            test_value_var = np.nanvar(test_data)

            mean_dic[c].append(test_value_mean)
            std_dic[c].append(test_value_std)
            var_dic[c].append(test_value_var)

        print(Fore.YELLOW + "iteration " + str(i) + Fore.BLUE + " done" + Style.RESET_ALL)

    for c in components:
        mean_dic_mean[c] = np.nanmean(mean_dic[c])
        std_dic_mean[c] = np.nanmean(std_dic[c])
        var_dic_mean[c] = np.nanmean(var_dic[c])
        # mean_sum = 0
        # std_sum = 0
        # var_sum = 0
        # for item in mean_dic[c]:
        #     mean_sum += item
        # for item in std_dic[c]:
        #     std_sum += item
        # for item in var_dic[c]:
        #     var_sum += item
        # mean_dic_mean[c] = mean_sum / len(mean_dic[c])
        # std_dic_mean[c] = std_sum / len(std_dic[c])
        # var_dic_mean[c] = var_sum / len(var_dic[c])

    return([mean_dic_mean, std_dic_mean, var_dic_mean])


def regression_test_example(template_name, accept_ieee=True, verbose=False, debug=False):
    status=True

    # regression_dir

    b = bootstrap(testdir='testingdir',
                  parameters_template_name=template_name,
                  keep_working_dir=debug, # if debug, keep working dir for analysis
                  accept_ieee=accept_ieee,
                  verbose=verbose,
                  debug=debug)


    utils = Util()

    meta = MetaReader(os.path.join(b.testdir, 'PiCoPiC.json'))

    el_charge = 1.6e-19
    rho_beam_scale = 1
    clim_estimation = meta.get_clim_estimation()
    shape=[0,0,100,500]
    temperature_shape=[90,1000,130,1200] # 90,130,1000,1200]

    timestamp=1e-8
    time_range=[meta.start_time, meta.end_time]

    use_cache=False
    use_grid=True
    cmap='terrain'
    temperature_component='electrons'

    clim_e_r = [-clim_estimation, clim_estimation]
    clim_e_z = [-clim_estimation, clim_estimation]
    clim_rho_beam = [-(meta.bunch_density * el_charge * rho_beam_scale), 0]
    autoselect = True
    x_axis_label = r'$\mathit{Z (m)}$'
    y_axis_label = r'$\mathit{R (m)}$'
    temperature_x_axis_label = r'$\mathit{t (s)}$'
    temperature_y_axis_label = r'$\mathit{T ( K^\circ )}$'
    e_x_axis_label = r'$\mathit{t (s)}$'
    e_y_r_axis_label = r'$\mathit{E_r (\frac{V}{m})}$'
    e_y_z_axis_label = r'$\mathit{E_z (\frac{V}{m})}$'
    cbar_axis_label = r'$\frac{V}{m}$'
    cbar_bunch_density_axis_label = r'$m^{-3}$'

    e_r_plot_name = r'$\mathbf{Electrical\enspace Field\enspace Radial\enspace Component}\enspace(E_r)$'
    e_z_plot_name = r'$\mathbf{Electrical\enspace Field\enspace Longitudal\enspace Component}\enspace(E_z)$'
    rho_beam_plot_name = r'$\mathbf{Electron\enspace Bunch\enspace Density}\enspace (\rho_{bunch})$'
    temperature_plot_name = r'$\mathbf{Temperature-time \enspace Dependency}\enspace(T)$'
    e_e_r_plot_name = r'$\mathbf{Electrical\enspace Field\enspace Radial\enspace Component}\enspace(E_r)$'
    e_e_z_plot_name = r'$\mathbf{Electrical\enspace Field\enspace Longitudal\enspace Component}\enspace(E_z)$'

    e_shape = [34, 341]

    r_scale = (shape[2] - shape[0]) / meta.number_r_grid
    z_scale = (shape[3] - shape[1]) / meta.number_z_grid

    if os.path.isfile(os.path.join(self.config_path, "data.h5")):
        reader = H5Reader(path = self.config_path, use_cache=use_cache, verbose=verbose)
    else:
        raise EnvironmentError("There is no corresponding data/metadata files in the path " + config_path + ". Can not continue.")

    ############################################################################

    plot = PlotBuilder(shape[3] - shape[1], shape[2] - shape[0],
                       fig_color=meta.figure_color,
                       fig_width=meta.figure_width,
                       fig_height=meta.figure_height,
                       fig_dpi=meta.figure_dpi,
                       font_family=meta.figure_font_family,
                       font_name=meta.figure_font_name,
                       font_size=meta.figure_font_size,
                       x_ticklabel_end=meta.z_size * z_scale, y_ticklabel_end=meta.r_size * r_scale,
                       tickbox=True, grid=use_grid, is_invert_y_axe=False,
                       aspect='equal', image_interpolation='nearest', guess_number_ticks=20)

    # add subplots
    plot.add_subplot_cartesian_2d(e_r_plot_name, 311, x_axe_label=x_axis_label, y_axe_label=y_axis_label)
    plot.add_subplot_cartesian_2d(e_z_plot_name, 312, x_axe_label=x_axis_label, y_axe_label=y_axis_label)
    plot.add_subplot_cartesian_2d(rho_beam_plot_name, 313, x_axe_label=x_axis_label, y_axe_label=y_axis_label)

    # add initial image with zeros and colorbar
    initial_image = np.zeros([shape[2] - shape[0], shape[3] - shape[1]])

    # add dummy images
    plot.add_image(e_r_plot_name, initial_image, cmap=cmap, clim=clim_e_r)
    plot.add_image(e_z_plot_name, initial_image, cmap=cmap, clim=clim_e_z)
    plot.add_image(rho_beam_plot_name, initial_image, cmap=cmap, clim=clim_rho_beam)

    # add colorbars
    plot.add_colorbar(e_r_plot_name, ticks=clim_e_r, title=cbar_axis_label)
    plot.add_colorbar(e_z_plot_name, ticks=clim_e_z, title=cbar_axis_label)
    plot.add_colorbar(rho_beam_plot_name, ticks=clim_rho_beam, title=cbar_bunch_density_axis_label)
    # plot.show()

    data_r = data_z = data_beam = []

    for probe in meta.probes:
        frame = meta.get_frame_number_by_timestamp(timestamp, probe.schedule)
        if (probe.type == 'frame') and (probe.r_start == shape[0]) and (probe.z_start == shape[1]) and(probe.r_end == shape[2]) and(probe.z_end == shape[3]):
            if probe.component == 'E_r': data_r = reader.get_frame('E_r', shape, frame)
            if probe.component == 'E_z': data_z = reader.get_frame('E_z', shape, frame)
            if probe.component == 'rho_beam': data_beam = reader.get_frame('rho_beam', shape, frame)

    # add timestamp to plot
    plot.get_figure().suptitle("Time: {:.2e} s".format(timestamp), x=.85, y=.95)

    # add images
    plot.add_image(e_r_plot_name, data_r, cmap=cmap, clim=clim_e_r)
    plot.add_image(e_z_plot_name, data_z, cmap=cmap, clim=clim_e_z)
    plot.add_image(rho_beam_plot_name, data_beam, cmap=cmap, clim=clim_rho_beam)

    plot.save(os.path.join(b.rootdir, 'image.png'))

    ############################################################################

    start_frame = None
    end_frame = None
    dump_interval = None
    temperature=[]

    temperature_r=[]
    temperature_phi=[]
    temperature_z=[]

    for probe in meta.probes:
        if (probe.type == 'mpframe') and (probe.component == temperature_component) and (probe.r_start == temperature_shape[0]) and (probe.z_start == temperature_shape[1]) and (probe.r_end == temperature_shape[2]) and (probe.z_end == temperature_shape[3]):
            dump_interval = probe.schedule
            start_frame = meta.get_frame_number_by_timestamp(time_range[0], dump_interval)
            end_frame = meta.get_frame_number_by_timestamp(time_range[1], dump_interval) - 1
            for i in range(start_frame, end_frame):
                el_sum_r=0
                el_sum_phi=0
                el_sum_z=0
                # calculating element
                r = np.fromfile("{}/{}/{}/mpframe_{}:{}_{}:{}/{}_vel_r.dat".format(meta.config_path, meta.data_root, probe.component, temperature_shape[0], temperature_shape[1], temperature_shape[2], temperature_shape[3], i), dtype='float', sep=' ')
                phi = np.fromfile("{}/{}/{}/mpframe_{}:{}_{}:{}/{}_vel_phi.dat".format(meta.config_path, meta.data_root, probe.component, temperature_shape[0], temperature_shape[1], temperature_shape[2], temperature_shape[3], i), dtype='float', sep=' ')
                z = np.fromfile("{}/{}/{}/mpframe_{}:{}_{}:{}/{}_vel_z.dat".format(meta.config_path, meta.data_root, probe.component, temperature_shape[0], temperature_shape[1], temperature_shape[2], temperature_shape[3], i), dtype='float', sep=' ')
                print("processing frame", i)
                for i in range(0, len(r)-1):
                    # el_sum_sq += r[i]*r[i]+phi[i]*phi[i]+z[i]*z[i]
                    el_sum_r += abs(r[i])
                    el_sum_phi += abs(phi[i])
                    el_sum_z += abs(z[i])

                # temperature.append(math.sqrt(el_sum_sq / len(r)))
                temperature_r.append(el_sum_r / len(r))
                temperature_phi.append(el_sum_phi / len(r))
                temperature_z.append(el_sum_z / len(r))

    data_timeline = np.linspace(time_range[0], time_range[1], len(temperature_r))

    # define plot builder
    plot = PlotBuilder(0, 0, # let the system detects sizes automatically
                       fig_color=meta.figure_color,
                       fig_width=meta.figure_width,
                       fig_height=meta.figure_height,
                       fig_dpi=meta.figure_dpi,
                       font_family=meta.figure_font_family,
                       font_name=meta.figure_font_name,
                       font_size=meta.figure_font_size,
                       tickbox=True, grid=use_grid, is_invert_y_axe=False,
                       aspect='auto', guess_number_ticks=20,
                       x_ticklabel_start=time_range[0],
                       x_ticklabel_end=time_range[1]
                      )

    # add subplots
    plot_t = plot.add_subplot_cartesian_2d(temperature_plot_name, 111,
                                           x_axe_label=temperature_x_axis_label,
                                           y_axe_label=temperature_y_axis_label)

    plot_t.plot(data_timeline, temperature_r, label="r")
    plot_t.plot(data_timeline, temperature_phi, label="phi")
    plot_t.plot(data_timeline, temperature_z, label="z")
    plot_t.legend(loc='upper left')

    plot.save(os.path.join(b.rootdir, 'image_temperature.png'))

    ############################################################################

    # get data
    start_frame = None # meta.get_frame_number_by_timestamp(time_range[0])
    end_frame = None # 460 # meta.get_frame_number_by_timestamp(time_range[1])

    data_r = data_z = []

    for probe in meta.probes:
        if (probe.type == 'dot') and (probe.r_start == e_shape[0]) and (probe.z_start == e_shape[1]):
            start_frame = meta.get_frame_number_by_timestamp(time_range[0], probe.schedule)
            end_frame = meta.get_frame_number_by_timestamp(time_range[1], probe.schedule) - 1
            if probe.component == 'E_r': data_r = reader.get_frame_range_dot('E_r', e_shape[0], e_shape[1], start_frame, end_frame)
            if probe.component == 'E_z': data_z = reader.get_frame_range_dot('E_z', e_shape[0], e_shape[1], start_frame, end_frame)

    data_timeline = np.linspace(time_range[0], time_range[1], len(data_r))

    # define plot builder
    plot = PlotBuilder(0, 0, # let the system detects sizes automatically
                       fig_color=meta.figure_color,
                       fig_width=meta.figure_width,
                       fig_height=meta.figure_height,
                       fig_dpi=meta.figure_dpi,
                       font_family=meta.figure_font_family,
                       font_name=meta.figure_font_name,
                       font_size=meta.figure_font_size,
                       tickbox=True, grid=use_grid, is_invert_y_axe=False,
                       aspect='auto', guess_number_ticks=20,
                       # number_x_ticks=10, number_y_ticks=10
                       x_ticklabel_end=1e-9, y_ticklabel_end=1e-9
                      )

    # add subplots
    plot_r = plot.add_subplot_cartesian_2d(e_e_r_plot_name, 121, x_axe_label=e_x_axis_label, y_axe_label=e_y_r_axis_label)
    plot_z = plot.add_subplot_cartesian_2d(e_e_z_plot_name, 122, x_axe_label=e_x_axis_label, y_axe_label=e_y_z_axis_label)

    plot_r.plot(data_timeline, data_r)
    plot_z.plot(data_timeline, data_z)

    plot.save(os.path.join(b.rootdir, 'image_e.png'))

    return status


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument('--type', type=str, help='collect_true_data, smoke, ext', default='smoke')
    parser.add_argument('--fastmath', action='store_true',
                        help='Use fast math, which are not compatible with IEEE calculation standard',
                        default=False)
    parser.add_argument('-v', '--verbose', action='count', default=0)
    parser.add_argument('-d', '--debug', action='count', default=0)
    parser.add_argument('--color-style', type=str,
                        help='Style of metadata colorization (works only with "--colorize" option)', default=None)

    args = parser.parse_args()

    ieee = not args.fastmath
    status = True

    if args.type == 'collect_true_data':
        status = collect_true_data('picopic.json.tmpl', 4,
                                   accept_ieee=ieee,
                                   verbose=args.verbose,
                                   debug=args.debug)
        mean_status = dumps(status[0], indent=2, sort_keys=True)
        std_status = dumps(status[1], indent=2, sort_keys=True)
        var_status = dumps(status[2], indent=2, sort_keys=True)
        if args.color_style:
            print(Fore.YELLOW
                  + "`Mean` mean values for 4th simulation iteraction:\n"
                  + Style.RESET_ALL,
                  highlight(mean_status, JsonLexer(),
                            Terminal256Formatter(style=args.color_style)))
            print(Fore.YELLOW
                  + "`Standard deviation` mean values for 4th simulation iteraction:\n"
                  + Style.RESET_ALL,
                  highlight(std_status, JsonLexer(), Terminal256Formatter(style=args.color_style)))
            print(Fore.YELLOW
                  + "`Variance` mean values for 4 simulationth iteraction:\n"
                  + Style.RESET_ALL,
                  highlight(var_status, JsonLexer(), Terminal256Formatter(style=args.color_style)))
        else:
            print(Fore.YELLOW
                  + "`Mean` mean values for 4th simulation iteraction:\n"
                  + Style.RESET_ALL,
                  mean_status)
            print(Fore.YELLOW
                  + "`Standard deviation` mean values for 4th simulation iteraction:\n"
                  + Style.RESET_ALL,
                  std_status)
            print(Fore.YELLOW
                  + "`Variance` mean values for 4th simulation iteraction:\n" + Style.RESET_ALL
                  + Style.RESET_ALL,
                  var_status)

    elif args.type == 'smoke':
        if ieee:
            rtol = 0.15
        else:
            rtol = 0.15
        for i in range(1, 5):
            print(Fore.GREEN + "Launching tests. Try " + str(i) + Style.RESET_ALL)
            status = test_example('picopic.json.tmpl', 4,
                                  accept_ieee=ieee, rel_tolerance=rtol,
                                  verbose=args.verbose,
                                  debug=args.debug)
            if status:
                break
            else:
                if i != 4: # do not print message at last failure
                    print(Fore.YELLOW + " Oops... try " + str(i) + " failed. Retrying during to random nature." + Style.RESET_ALL)
                else:
                    continue

    elif args.type == 'ext':
        if ieee:
            rtol = 0.18
        else:
            rtol = 0.2
        status = test_example('picopic.json.tmpl', 11,
                              accept_ieee=ieee, rel_tolerance=rtol,
                              verbose=args.verbose,
                              debug=args.debug)
    elif args.type == 'regression':
        status = regression_test_example('picopic.json.tmpl',
                                         accept_ieee=ieee,
                                         verbose=args.verbose,
                                         debug=args.debug)
    else:
        raise Exception("there is no test type {}".format(args.type))

    if status:
        if args.type != 'collect_true_data':
            print(Fore.BLUE + "Test PASSED" + Style.RESET_ALL)
        exit(0)
    else:
        if args.type != 'collect_true_data':
            print(Fore.RED + "Test FAILED" + Style.RESET_ALL)
        exit(1)
