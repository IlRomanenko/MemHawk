# Heaptrack mode

MemHawk can write raw heaptrack data without a symbolification step. Such output is enabled if option `memhawk.writers.heaptrack_writer.enabled` is set to `true`, right now it's **enabled** by default.

## Usage

### Step 1: Run the application with MemHawk

```bash
MEMHAWK_OPTS=memhawk.writers.heaptrack_writer.enabled=true LD_PRELOAD=./libmemhawk.so filelight
```

#### Step 2: Symbolize raw data from MemHawk

After starting filelight, MemHawk will create a file with raw heaptrack data. Before ingesting it into heaptrack GUI, data should be processed and symbolized. It can be done with the `heaptrack_interpret` utility.

```bash
time /usr/lib/heaptrack/libexec/heaptrack_interpret <memhawk_filelight_<pid>_heaptrack.txt >heaptrack.data
```

**N.B.** For AppImage installations, such an archive should be unpacked in order to get access to plain binaries, just run `heaptrack.AppImage --squash-fs`

#### Step 3: Start heaptrack with analyzed data

Start heaptrack application with `-a` flag.

```bash
/usr/bin/heaptrack -a heaptrack.data
```
