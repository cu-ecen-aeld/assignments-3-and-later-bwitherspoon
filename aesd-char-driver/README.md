# AESD Char Driver

Template source code for the AESD char driver used with assignments 8 and later

## Debugging

See https://www.kernel.org/doc/html/next/core-api/printk-basics.html

```shell
cat /proc/sys/kernel/printk
echo 8 > /proc/sys/kernel/printk
```

```shell
sysctl kernel.printk
```

```shell
journalctl -f -k
```

```shell
make DEBUG=y
sudo ./aesdchar_load
```

