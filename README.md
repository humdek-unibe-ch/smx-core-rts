# Streamix RTS `smxrts`

The runtime system (RTS) library for the coordination language Streamix.
This library is used by any Streamix box which together with this runtime system may form a Streamix application.

In short, the runtime system spawns box instances as threads and manages the communication amongst the box instances.

## Installation

The release page offers pre-compiled debian packages which can be installed through command line.

Note that time-triggered nets are executed with RT-Tasks.
In order for this to work, the user executing the Streamix application requires the appropriate rights.
These can be configured in `/etc/security/limits.conf`.
E.g. add the line `localtest       -       rtprio          99` in order to allow the user `localtest` to run Streamix applications with RT priority.

## Building

To build the RTS from scratch run the following commands:

```sh
make
sudo make install
```


## Dependencies
- [`smx-dep-zlog`](https://github.com/humdek-unibe-ch/smx-dep-zlog)
    This library wraps the publicly available [`zlog`](https://github.com/HardySimpson/zlog) library in order to facilitate installing processes.
    Note that the zlog library was altered slighly to accommodate for a potential priority-inversion problem when using RT threads.

 - [`libbson`](http://mongoc.org/libbson/current/index.html)

    ```
    sudo apt install libbson-dev
    ```

 - [`pthread`](https://computing.llnl.gov/tutorials/pthreads/)

    ```
    sudo apt install libpthread-stubs0-dev
    ```

 - [`lttng`](https://lttng.org/)

    ```
    sudo apt-get install lttng-tools
    sudo apt-get install lttng-modules-dkms
    ```

## Streamix Applications

Look for repositories with the name prefix `smx-app-` for Streamix applications which were built with this runtime system.
Try [this link](https://github.com/search?q=topic%3Astreamix-app+org%3Ahumdek-unibe-ch+fork%3Atrue&type=repositories) for a list of available Streamix apps.
