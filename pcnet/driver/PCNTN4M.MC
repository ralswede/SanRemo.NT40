;
;//File Name: failover.mc
;//
;//  Note: comments in the .mc file must use both ";" and "//".
;//
;//  Status values are 32 bit values layed out as follows:
;//
;//   3 3 2 2 2 2 2 2 2 2 2 2 1 1 1 1 1 1 1 1 1 1
;//   1 0 9 8 7 6 5 4 3 2 1 0 9 8 7 6 5 4 3 2 1 0 9 8 7 6 5 4 3 2 1 0
;//  +---+-+-------------------------+-------------------------------+
;//  |Sev|C|       Facility          |               Code            |
;//  +---+-+-------------------------+-------------------------------+
;//
;//  where
;//
;//      Sev - is the severity code
;//
;//          00 - Success
;//          01 - Informational
;//          10 - Warning
;//          11 - Error
;//
;//      C - is the Customer code flag
;//
;//      Facility - is the facility code
;//
;//      Code - is the facility's status code
;//
;
MessageIdTypedef=NTSTATUS

SeverityNames=(Success=0x0:STATUS_SEVERITY_SUCCESS
               Informational=0x1:STATUS_SEVERITY_INFORMATIONAL
               Warning=0x2:STATUS_SEVERITY_WARNING
               Error=0x3:STATUS_SEVERITY_ERROR
              )

FacilityNames=(System=0x0
               RpcRuntime=0x2:FACILITY_RPC_RUNTIME
               RpcStubs=0x3:FACILITY_RPC_STUBS
               Io=0x4:FACILITY_IO_ERROR_CODE
	       FailOver=0x7:FACILITY_PCNTN4M_ERROR_CODE
              )

;
;//
;// %1 is reserved by the IO Manager. If IoAllocateErrorLogEntry is
;// called with a device, the name of the device will be inserted into
;// the message at %1. Otherwise, the place of %1 will be left empty.
;// In either case, the insertion strings from the driver's error log
;// entry starts at %2. In other words, the first insertion string goes
;// to %2, the second to %3 and so on.
;//
;

MessageId=0x0001 Facility=FailOver Severity=Warning SymbolicName=FAILOVER_1
Language=English
PermaNet(tm) Server : No Secondary Adapter Found. Redundant Mode is disabled.
.

MessageId=0x0002 Facility=FailOver Severity=Error SymbolicName=FAILOVER_2
Language=English
PermaNet(tm) Server : Problem occurs on the Primary adapter.
Switching over to the Secondary Adapter.
.

MessageId=0x0003 Facility=FailOver Severity=Informational SymbolicName=FAILOVER_3
Language=English
PermaNet(tm) Server : Switching back to the Primary Adapter.
.


