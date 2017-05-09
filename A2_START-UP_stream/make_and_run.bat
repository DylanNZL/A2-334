cls
@echo off

@echo RServer_UDP_ipv6/make
cd RServer_UDP_ipv6
make
cd ..

@echo RClient_UDP_ipv6/make
cd RClient_UDP_ipv6
make
cd ..

@echo Available Tests:
@echo 1) 0 0 No corrupion or packet loss
@echo 2) 0 1 No corrupion and packet loss
@echo 3) 1 0 Corruption and no packet loss
@echo 4) 1 1 Corruption and packet loss
@echo Which Test:
set /p selection=
@echo %selection%

if %selection% == 1 (
  START cmd /k "RServer_UDP_ipv6\Rserver_UDP 1235 0 0"
  START cmd /k "RClient_UDP_ipv6\Rclient_UDP localhost 1235 0 0"
  pause
) else if %selection% == 2 (
  START cmd /k "RServer_UDP_ipv6\Rserver_UDP 1235 0 1"
  START cmd /k "RClient_UDP_ipv6\Rclient_UDP localhost 1235 0 1"
  pause
) else if %selection% == 3 (
  START cmd /k "RServer_UDP_ipv6\Rserver_UDP 1235 1 0"
  START cmd /k "RClient_UDP_ipv6\Rclient_UDP localhost 1235 1 0"
  pause
) else if %selection% == 4 (
  START cmd /k "RServer_UDP_ipv6\Rserver_UDP 1235 1 1"
  START cmd /k "RClient_UDP_ipv6\Rclient_UDP localhost 1235 1 1"
  pause
)
