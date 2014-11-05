Energy Measurement Library
==========================
The Energy Measurement Library provides a simple API to extract energy
consumption data from hardware power counters. It is designed to simplify energy
consumption experimentation by presenting a unified interface for different
kinds of hardware.

The following devices are currently supported:

	* Intel CPUs starting from Sandy Bridge
	(through the Running Average Power Limit interface)

	* Recent Nvidia Tesla and Quadro GPUs
	(through NVIDIA Management Library)

	* Intel Xeon Phi MICs
	(through the Intel Manycore Platform Software Stack (3.x+), from the host device)

	* Schleifenbauer PDUs
	(through the Schleifenbauer socket API)

EML automatically discovers necessary libraries and available devices at runtime.

Documentation
-------------
Doxygen-generated documentation can be found at:

https://hpc-ull.github.io/eml

License
-------
EML is released under the GPLv2 license.

Dependencies
------------
Required:
* [libconfuse](https://github.com/martinh/libconfuse), for configuration parsing

Optional:
* [NVML](https://developer.nvidia.com/nvidia-management-library-nvml), for Nvidia GPU support
* [MPSS 3.x+](https://software.intel.com/en-us/articles/intel-manycore-platform-software-stack-mpss), for Intel MIC support
* [libcrypto](https://www.openssl.org), for Schleifenbauer PDU support (RC4 encryption)

Contact
-------
Universidad de La Laguna, High Performance Computing group <cap@pcg.ull.es>

Homepage: http://cap.pcg.ull.es

Acknowledgements
----------------
This work was supported by the Spanish Ministry of Education and Science through 
TIN2011-24598 project, through the FPU program and the Spanish network CAPAP-H4. 
It also has been partially supported by EU under the COST programme Action IC1305, 
‘Network for Sustainable Ultrascale Computing (NESUS)’
