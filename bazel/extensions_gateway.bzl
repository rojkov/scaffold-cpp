# Flat list of gateway extension Bazel labels.
# Comment out a line to exclude the extension from the gateway binary.
GATEWAY_EXTENSIONS = [
    "//src/extensions/gateway/task_sources/http:http",
    "//src/extensions/gateway/schedulers/round_robin:round_robin",
    "//src/extensions/gateway/discovery/static:static",
]
