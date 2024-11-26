# Protocolos de Comunicación - TPE

## Authors

- [Lucas Lonegro Gurfinkel](https://github.com/LucasLonegro)
- [Tobías Noceda](https://github.com/Tobias-Noceda)
- [Tomás Raiti](https://github.com/Secreto31126)
- [Franco Ghigliani](https://github.com/Franco-A-Ghigliani)

## Project

Implementation of a server module for the POP3 protocol, design and implementation of a bespoke protocol for the administration of this module and a companion interface for the latter implementation.

## Compiling

Compiling is done in the project root with the command:

```bash
make all DEBUG=0
```

## Execution

In the ```./dist``` directory, compilation generates the following executables:

* `server`
* `manager`

To execute them from the project root:

```bash
./dist/server <flags>
```

Execution flags are as follows

| Flag |  Description | 
|----| -------------------------------------------------------------------------------------------------------------------------- |
| -h | Prints the help menu and terminates. |
| -l \<POP3 addr\> | Sets the address to serve the POP3 server. By default listens on all interfaces. |
| -L \<conf addr\> | Sets the address to serve the management service. By defect listens on the loopback interface. |
| -p \<POP3 port\> | Sets the incoming port for POP3 connections. By default the port is 110. |
| -P \<conf port\> | Sets the incoming port for management connectinos. By default the port is 4321 |
| -u \<name\>:\<pass\> | List of users and passwords recognized by the server. The maximum value is 10. |
| -a \<name\>:\<pass\> | List of admin users and passwords recognized by the server. The maximum is 4. |
| -t \<cmd\> | Sets a transformer/filter program for output. The default program is `cat`. |
| -d \<dir\> | Specifies the directory in which Maildirs are located. The default value is `./dist/mail` |
| -v | Prints version information and terminates. |


```bash
./dist/manager <command>
```

Execution flags are as follows

| Flag |  Description | 
|----| -------------------------------------------------------------------------------------------------------------------------- |
| -P | Sets the port for the maangement service. Default value is 4321. |
| -L | Sets the address for the management service. Default values is the loopback address. |

## Cleaning

Build files can be cleaned with the command:

```bash
make clean
```
