#include "lib.hpp"

using namespace constant;

namespace lib
{
  // C^2 define c^2 to decrease number of operations
  const double LIGHT_VEL_POW_2 = pow (LIGHT_VEL, 2);

#if __cplusplus >= 201103L
  using std::isnan;
#endif

#ifdef __AVX__
  double sqrt_recip(double x)
  {
    //! 1/sqrt(x), using AVX squared root procedure
    double d[4];
    __m256d c = _mm256_sqrt_pd(_mm256_set_pd(x, 0, 0, 0));

    _mm256_store_pd(d, c);

    return d[3];
  }
#endif

  double sq_rt(double x)
  {
#if defined (__AVX__) // && defined (EXPERIMENTAL)
    return sqrt_recip(x);
#else
    return sqrt(x);
#endif
  }

  bool to_bool(string str)
  {
    std::transform(str.begin(), str.end(), str.begin(), ::tolower);
    std::istringstream is(str);
    bool b;
    is >> std::boolalpha >> b;
    return b;
  }

  double get_gamma (double sq_velocity)
  {
    //! get_gamma takes squared velocity,
    //! only because of some features of code
    //! and optimisation issues
    double gamma, beta;

    beta = sq_velocity / LIGHT_VEL_POW_2;

    if (beta > 1) // it's VERY BAD! Beta should not be more, than 1
      LOG_CRIT("(get_gamma): Lorentz factor aka gamma is complex. Velocity is: " << sq_rt(sq_velocity), 1);

    gamma = 1 / sq_rt(1.0 - beta);

    if (isinf(gamma) == 1)
    { // avoid infinity values
      LOG_WARN("(get_gamma): gamma (Lorenz factor) girects to infinity for velocity" << sq_rt(sq_velocity));
      return 1e100; // just return some very big value
    }
    else
      return gamma;
  }

  double get_gamma_inv (double sq_velocity)
  {
    //! get_gamma_inv takes squared velocity,
    //! only because of some features of code
    //! and optimisation issues
    double gamma = pow(1.0 + sq_velocity / LIGHT_VEL_POW_2, -0.5);

    return gamma;
  }

  char* get_simulation_duration()
  // get the time, spent since simulation launched (in "d h m s" format)
  {
    double time_sec = std::time(nullptr) - SIMULATION_START_TIME;

    int d, hr, min;
    double sec;
    char* the_time = new char;

    int sec_in_min = 60;
    int sec_in_hr = 3600;
    int sec_in_day = 86400;

    if (time_sec > sec_in_min && time_sec < sec_in_hr)
    {
      min = (int)time_sec / sec_in_min;
      sec = time_sec - min * sec_in_min;
      sprintf(the_time, "%dm %.0fs", min, sec);
    }
    else if (time_sec > sec_in_hr && time_sec < sec_in_day)
    {
      hr = (int)time_sec / sec_in_hr;
      min = (int)((time_sec - hr * sec_in_hr) / sec_in_min);
      sec = time_sec - hr * sec_in_hr - min * sec_in_min;
      sprintf(the_time, "%dh %dm %.0fs", hr, min, sec);
    }
    else if (time_sec > sec_in_day)
    {
      d = (int)time_sec / sec_in_day;
      hr = (int)((time_sec - d * sec_in_day) / sec_in_hr);
      min = (int)((time_sec - d * sec_in_day - hr * sec_in_hr) / sec_in_min);
      sec = time_sec - d * sec_in_day - hr * sec_in_hr - min * sec_in_min;
      sprintf(the_time, "%dd %dh %dm %.0fs", d, hr, min, sec);
    }
    else
      sprintf(the_time, "%.0fs", time_sec);

    return the_time;
  }

// Make directory and check if it exists
  bool directory_exists(const std::string& path)
  {
#ifdef _WIN32
    struct _stat info;
    if (_stat(path.c_str(), &info) != 0)
      return false;
    return (info.st_mode & _S_IFDIR) != 0;
#else
    struct stat info;
    if (stat(path.c_str(), &info) != 0)
      return false;
    return (info.st_mode & S_IFDIR) != 0;
#endif
  }

  bool make_directory(const std::string& path)
  {
#ifdef DEBUG
    LOG_DBG("Creating directory " << path);
#endif

#ifdef _WIN32
    int ret = _mkdir(path.c_str());
#else
    mode_t mode = 0755;
    int ret = mkdir(path.c_str(), mode);
#endif
    if (ret == 0)
      return true;

    switch (errno)
    {
    case ENOENT:
      // parent didn't exist, try to create it
    {
      long unsigned int pos = path.find_last_of('/');
      if (pos == std::string::npos)
#ifdef _WIN32
        pos = path.find_last_of('\\');
      if (pos == std::string::npos)
#endif
        return false;
      if (!make_directory(path.substr(0, pos)))
        return false;
    }
    // now, try to create again
#ifdef _WIN32
    return 0 == _mkdir(path.c_str());
#else
    return 0 == mkdir(path.c_str(), mode);
#endif

    case EEXIST:
      // done!
      return directory_exists(path);

    default:
      return false;
    }
  }

  char *get_cmd_option(char **begin, char **end, const std::string &option)
  {
    char * *itr = std::find(begin, end, option);
    if (itr != end && ++itr != end)
      return *itr;
    return 0;
  }

  bool cmd_option_exists(char **begin, char **end, const std::string &option)
  {
    return std::find(begin, end, option) != end;
  }

  std::vector<double> read_file_to_double(const char *filename)
  {
    std::ifstream ifile(filename, std::ios::in);
    std::vector<double> scores;

    //check to see that the file was opened correctly:
    if (!ifile.is_open())
      LOG_CRIT("Can not open file ", -1);

    double num = 0.0;
    //keep storing values from the text file so long as data exists:
    while (ifile >> num)
      scores.push_back(num);

    //verify that the scores were stored correctly:
    // for (int i = 0; i < scores.size(); ++i) {
    //   std::cout << scores[i] << std::endl;
    // }

    return scores;
  }
}