# Kuksa protos

`proto/` is a `repo` project managed by the manifest, checked out from:

* Repository: https://github.com/eclipse-kuksa/kuksa-proto
* License: Apache-2.0

Do not edit anything under `proto/` — it is upstream-owned and `repo sync` will overwrite it.

To move to a newer version, bump `revision` on the `kuksa-proto` project in the manifest
(`custom-additions.xml`) and re-run `repo sync`.

The C++ and gRPC stubs are generated at build time by the `KuksaValProtoStub_h` /
`KuksaValProtoStub_cc` genrules in the module `Android.bp`.
