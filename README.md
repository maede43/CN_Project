# CN_Project
Computer Networking Project - Spring Semester 2021

### check port is open
compile :
```
gcc check_port_is_open.c -o check_port_is_open -pthread
```
usage :
```
    -h|--host <host>|Target host(IPv4)
    -a|--all-ports
    -w|--well-known-ports
    -s|--specified_port <port name> like [-s 'http']
    -r|--threads <num>|Number of threads (default: 8)
    -t|--timeout <num>|Timeout is sec (default: 3)
    -p|--port-range|Range of port numbers
```
example :
```
./check_port_is_open -h 8.8.8.8 -w
./check_port_is_open -h 8.8.8.8 -s http
```

### ping
compile : 
```
gcc ping.c -o ping -pthread
```
usage :
```
Usage: sudo ./ping [options] <host(s)>
  Options:
    -s|--size <num>|Size of packet (default: 64))
    -t|--timeout <num>|Timeout is sec (default: 1)
```
example :
```
sudo ./ping Instagram.com 8.8.8.8
```

### traceroute tcp icmp
compile :
```
gcc traceroute_tcp_icmp.c -o traceroute_tcp_icmp
```
usage :
```
Usage: sudo ./traceroute_tcp_icmp -d <host> [options]
  Options:
    -m|--max-ttl <num>|Max ttl (default: 30))
    -t|--timeout <num>|Timeout is sec (default: 10)
    -s|--start-ttl <num>|Start ttl (default: 1)
    -p|--port-number <num>|Port number (default: 12564)
    -a|--tries-number <num>|Tries number (default: 1)
```
example :
```
sudo ./traceroute_tcp_icmp -d aparat.com
```

### get macs
compile : 
```
gcc getmacs.c -o getmacs
```
usage :
```
Usage: getmacs [-t time] [-r IP/CIDR] interface

  -r IP/CIDR      Scan this IP range. If not given <localIP>/24 is used
  -t time         Set wait-for-reply timeout to <time> milliseconds. Default is 950 ms
  -h              Print this help
```
example :
```
sudo ./getmacs -r 192.168.0.1/30 ens33
```
