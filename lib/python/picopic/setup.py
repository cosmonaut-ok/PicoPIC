from setuptools import setup
# from distutils.core import setup

setup(name='picopic',
      version='0.0.1',
      description='Data analysis tools for PicoPIC project',
      author='Alexander Vynnyk',
      author_email='alexander.vynnyk@gmail.com',
      url='https://gitlab.com/my-funny-plasma/PIC/picopic',
      install_requires=[
          'matplotlib', 'numpy', 'scipy', 'h5py', 'setupext-janitor'
      ],
      zip_safe=False)