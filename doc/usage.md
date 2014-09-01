Usage
=====
To measure energy consumption with EML, applications are instrumented with
library calls which mark the target sections of code.

An example of the common usage pattern (albeit with no error checking) would
be:

~~~
	#include <eml.h>
	#include <stdlib.h> //size_t

	int main() {
	  emlInit();

	  //allocate space for results
	  size_t count;
	  emlDeviceGetCount(&count);
	  emlData_t* data[count];

	  //start measuring energy consumption
	  emlStart();

	  //do work...

	  //stop measuring and retrieve results
	  emlStop(data);

	  //use data...

	  emlShutdown();
	}
~~~

First, an application needs to initialize the library through a call to @ref
emlInit. This triggers automatic discovery of all devices supported by EML, as
well as any necessary runtime support (such as third-party libraries).

Code sections are then wrapped with calls to @ref emlStart and @ref emlStop.
Each call to @ref emlStart signals the beginning of monitoring until the next
call to @ref emlStop. This monitoring is performed in the background through
data-collecting threads.

@ref emlStop returns an array of @ref emlData\_t references, one for each
measured device. This is used to extract totals or dump the entire dataset. To
allocate the necessary space, the number of available devices can be determined
through @ref emlDeviceGetCount.

When the library is no longer needed, its resources can be freed through @ref
emlShutdown.

Section nesting
---------------
Code sections can be nested by making successive calls to @ref emlStart before
calling @ref emlStop. This makes it possible to write code such as:

~~~
	emlStart(); //measure whole loop
	for (int i = 0; i < n; i++) {
		emlStart(); //measure each iteration
		do_work(i);
		emlStop(&iterationdata[i]);
	}
	emlStop(&loopdata);
~~~

Internally, only one monitoring thread per device will be spawned for a section
containing nested calls, making them no more expensive than a single section.

Per-device measurements
-----------------------
It is also possible to measure on a specific subset of devices through the
family of device-specific functions (mainly @ref emlDeviceStart and @ref
emlDeviceStop).

These functions operate on @ref emlDevice\_t references, which can be obtained
through @ref emlDeviceByIndex as follows:

~~~
	size_t count;
	emlDeviceGetCount(&count);
	printf("Found %zu devices:\n", count);

	emlDevice_t* dev;
	const char* devname;
	for (size_t i = 0; i < count; i++) {
	  emlDeviceByIndex(i, &dev);
	  emlDeviceGetName(dev, &devname);

	  printf("Device %zu: %s\n", i, devname);
	}
~~~

Result processing
-----------------
Measurement datasets contain a time series of raw datapoints (dependent on
device type). From the C API, only two basic functions are available: @ref
emlDataGetConsumed and @ref emlDataGetElapsed. These return energy and time
totals for the dataset, abstracting away any necessary computation over the raw
values to convert readings between power and energy, or between different units:

~~~
	double consumed, elapsed;
	emlDataGetConsumed(data, &consumed);
	emlDataGetElapsed(data, &elapsed);
	emlDataFree(data);

	printf("This device consumed %g J in %g s\n", consumed, elapsed);
~~~

For further processing, the entire dataset can be exported as JSON through @ref
emlDataDumpJSON. The description of the serialization format can be found on
[JSON serialization](doc/json.md).
