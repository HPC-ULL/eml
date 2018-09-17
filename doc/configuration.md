Configuration
=============
The behavior of the library can be controlled through a config file.
It can be used to

* change measurement parameters, such as sampling rate
* declare devices that cannot be autodetected (mainly network-based devices)
* completely disable support for specific device types at runtime

The config file is optional.  If not provided, the library will fall back on
sane parameter defaults, and only autodetected devices will be available for
measurement.

Loading
-------
During @ref emlInit, the library looks for a config file in the following paths,
in order:

*  1. <tt>$XDG\_CONFIG\_HOME/eml/config</tt>
      (if @c $XDG\_CONFIG\_HOME is set and not empty)
*  2. <tt>$HOME/.config/eml/config</tt>
*  3. <tt>/etc/eml/config</tt>

Only the first existing file is parsed, even if it is malformed (in which case
it will be ignored).

Format
------
The config file is parsed using @c libconfuse.

Each driver is configured in a separate section:
~~~
# slower gpu sampling (1 sec)
nvml {
  sampling_interval = 1000000000
}

# we don't need cpu measurements
rapl {
  disabled = true
}

# Nested configurations are configured as follows
sb-pdu {

}
~~~
Default values will be used for any missing entries, except for values made
necessary for existing entries (such as the hostname field for a network device
that is declared by the configuration). In particular, loading an empty config
file has no effect.

Available Configuration Values
------------------------------
The driver configuration section is the .name value defined in its specified driver struct.

- Common Values
  - **disabled**. Determines wether to use the module or not.<br/>
    Values: true, false.<br/>
    Default: false, except for the dummy module.
  - **sampling_interval**. Determines the sampling interval for the driver. Check each module for its default value.
    Values: numerical value of nanoseconds. 100000000 = 100ms, 100000 = 100μs

- dummy (Dummy testing driver) 
  - **disabled**. This module is disabled by default.<br/>
    Default: true
  - **sampling_interval**.<br/>
    Default: 100000000, i.e. 100ms.

- rapl (Intel RAPL)
  - **sampling_interval**.<br/> 
    Default: 1000000, i.e. ~1ms.

- nvml (Nvidia Management Library) 
  - **sampling_interval**.<br/> 
    Default: 16000000, i.e. ~16ms. Adjusted for Fermi power readings.

- mic (Intel MIC)
  - **sampling_interval**.<br/> 
    Default: 50000000, i.e. ~50ms. 

- sb-pdu (Schleifenbauer PDUs)
  - **sampling_interval**.<br/> 
    Default: 1000000000, i.e. ~1s.
  - **device**. SBPDU device specification.
    - **host**. Host IPv4.<br/>
    Default: 192.168.1.200
    - **port**.<br/>
    Default: 7783
    - **rc4key**. RC4 encryption key.<br/>
    Default: 000000000000

- odroid (Odroid-XU3)
  - **sampling_interval**.<br/> 
    Default: 263808000, i.e. ~263808μs as defined by /sys/bus/i2c/drivers/INA231/*/update_period.

- labee (Poznań Supercomputing and Networking Center Labee XML Interface). Currently only one device per node is supported.
  - **sampling_interval**.<br/> 
    Default: 150000000, i.e. ~150ms.
  - **hostname**. Name of the host to measure.<br/>
    Default: ""
  - **nodelist_file**. Path to the file with the hardware labels mapped to hostnames as comma separated values.<br/>
    Example file:<br/>
~~~
hw-id1,hostname1
hw-id2,hostname2
hw-id3,hostname3
hw-id4,hostname4
~~~
    Default: ./nodelist
  - **api_url**. Rest API full url.<br/>
    Default: http://10.11.12.242/REST/node
  - **user**. API username.<br/>
    Default: user
  - **password**. API password.<br/>
    Default: password
  - **power_attribute**. Key in the XML response that contains the measurement we want to query.<br/>
    Default: actualPowerUsage


