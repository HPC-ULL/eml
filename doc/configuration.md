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
~~~
For available values, please check the driver code for now.

Default values will be used for any missing entries, except for values made
necessary for existing entries (such as the hostname field for a network device
that is declared by the configuration). In particular, loading an empty config
file has no effect.
