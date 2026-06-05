# Resource manager

Provides resource info for other modules.

It provides the following interfaces:

* [aos::sm::resourcemanager::ResourceInfoProviderItf](itf/resourceinfoprovider.hpp) - provides resources info.

## aos::sm::resourcemanager::ResourceInfoProviderItf

### GetResourcesInfos

Returns resource infos.

## Resource info file

The resource manager reads resource definitions from a JSON file that follows
the [schema definition](https://github.com/aosedge/aos_protocols/blob/main/core/aos-resources-config.schema.json).
By default, it looks for the resource info file at `/etc/aos/resources.cfg`, but the Service Manager
configuration file (`/etc/aos/sm.cfg`) can override this by specifying `resourcesConfigFile`.

## Example

This example defines two resources: `kuksa` which adds an entry to `/etc/hosts`, and
`camera0` which passes through video devices and belongs to the `video` group.
The `camera0` resource can be shared by up to 2 instances simultaneously.

### Aos resource info file

```json
[
  {
    "name": "kuksa",
    "hosts": [
      {
        "ip": "10.0.0.100",
        "hostname": "Server"
      }
    ]
  },
  {
    "name": "camera0",
    "devices": [
      "/dev/video0",
      "/dev/video1",
      "/dev/media0:/dev/media-link0"
    ],
    "groups": [
      "video"
    ],
    "sharedCount": 2
  }
]
```

Device paths can be specified in the format `src:dst`, where `src` is the device path
on the host and `dst` is the device path inside the container
(see `/dev/media0:/dev/media-link0` in the example above).
If `dst` is omitted, it defaults to the same path as `src`.

### Aos item configuration

The resources defined in the resource info file can be referenced in Aos item configurations as follows:

```yaml
configuration:
  resources:
    - name: camera0
      mode: rw
    - name: kuksa
```

`mode` controls the access mode applied to the resource's devices (e.g. `ro`, `rw`).
Empty `mode` will result in no access mode applied.
