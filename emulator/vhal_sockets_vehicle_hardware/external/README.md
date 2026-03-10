# Socket based VHAL Hardware

This socket base VHAL hardware implementation is not meant for production use. It is supposed to
connect to the [Mock WS server](https://github.com/COVESA/aosp_vhal_server_mock) to enable the
emulator to run Apps using the OEM VHAL extension.

## How to use

1) Clone https://github.com/dhbaird/easywsclient into this directory (done by repo):

```
|- device
  |- generic
    |- car
      |- emulator
        |- vhal_socket_vehicle_hardware
          |- external
            |- easuwsclient
              |- easywsclient.cpp
              |- easywsclient.hpp
              |- ...
```

2) Apply patch:

```bash
    $ cd external/easywsclient
    $ git apply ../easywsclient_fix.patch
```