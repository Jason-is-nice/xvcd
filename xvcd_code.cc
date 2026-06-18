
xvcServer_v1.0:2048  //确认下单位

int main(int argc, char **argv)

	int io_init(int product, int vendor)
	
	WSAStartup
	setsockopt
	bind
	listen
	
	select
	
	accept
	
	int handle_data(int fd)
	

int handle_data(int fd)
	const char xvcInfo[] = "xvcServer_v1.0:2048\n"; 
	unsigned char buffer[2*2048], result[2*1024];
	
	setsockopt
	sread
	
	if (memcmp(cmd, "ge", 2) == 0)
		memcpy(result, xvcInfo, strlen(xvcInfo));
		send(fd, result, strlen(xvcInfo), 0) != strlen(xvcInfo)
	
	else if (memcmp(cmd, "se", 2) == 0)
		send(fd, result, 4, 0) != 4)
	
	else if (memcmp(cmd, "sh", 2) == 0)
	
	// Read socket complete
	QueryPerformanceCounter(&qpc_times[qpc_inx++]);
	
	if (sread(fd, buffer, nr_bytes * 2) != 1)
	
	
	int io_scan(const unsigned char *TMS, const unsigned char *TDI, unsigned char *TDO, int bits)
		CHK_STAT(FT_Write(sFTDI_fd, buffer, bytes_to_write, &bytes_written));
		         FT_Write( FT_HANDLE ftHandle, LPVOID lpBuffer, DWORD dwBytesToWrite, LPDWORD lpBytesWritten);
		
		
	if (send(fd, result, nr_bytes, 0) != nr_bytes)
	


//
// Only allow exiting if the state is rti and the IR
// has the default value (IDCODE) by going through test_logic_reset.
// As soon as going through capture_dr or capture_ir no exit is
// allowed as this will change DR/IR.
仅当状态为rti且IR具有默认值（IDCODE）时，通过test_logic_reset允许退出。
一旦进入capture_dr或capture_ir阶段，即不允许退出，因为这将改变DR/IR。


// Due to a weird bug(??) xilinx impacts goes through another "capture_ir"/"capture_dr" cycle after
// reading IR/DR which unfortunately sets IR to the read-out IR value.
// Just ignore these transactions.

由于一个奇怪的错误，xilinx impacts 在读取ir/dr后会经历另一个“capture_ir”/“capture_dr”循环，不幸的是，这会将ir设置为读取的ir值。
忽略这些交易。
