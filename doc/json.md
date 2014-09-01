JSON serialization
==================
Energy consumption datasets can be exported as JSON files through @ref
emlDataDumpJSON. An @c %emlData JSON object contains a time series of energy
data, as well as measurement metadata.

To avoid loss of precision, all values are reported in raw format (that is, in
the units native to the method used to obtain the readings), along with
rational unit factors to convert them to base SI units.

JSON schema
-----------
@c %emlData objects shall conform to the following schema:

@include emlData.schema.json
