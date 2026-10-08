from setuptools import find_packages, setup

package_name = 'pi1_package'

setup(
    name=package_name,
    version='0.0.0',
    packages=find_packages(exclude=['test']),
    data_files=[
        ('share/ament_index/resource_index/packages',
            ['resource/' + package_name]),
        ('share/' + package_name, ['package.xml']),
    ],
    install_requires=['setuptools'],
    zip_safe=True,
    maintainer='eddy',
    maintainer_email='eddypetrenko11@gmail.com',
    description='Communication package for the onboard raspberry pi',
    license='Apache-2.0',
    extras_require={
        'test': [
            'pytest',
        ],
    },
    entry_points={
        'console_scripts': [
            'publisher = pi1_package.publisher_node:main',
            'subscriber = pi1_package.subscriber_node:main',
        ],
    },
)
