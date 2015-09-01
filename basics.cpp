#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <ctype.h>
#ifdef __WINE__
#include "wine/debug.h"
#endif

#include "spifns.h"
#include "basics.h"
#include "spi.h"
#include "compat.h"
#include "logging.h"

/*
 * README:
 * I attempted to reverse engineer spilpt.dll the best I could.
 * However, compiling it with visual studio 2010 did not allow me to yield an identical DLL, and I don't have VS 2005 (original was compiled with this) installed.
 * For now, I'll leave it at this 
 * Feel free to use this to write your own 
*/

#ifdef __WINE__
WINE_DEFAULT_DEBUG_CHANNEL(spilpt);
#else
#define WINE_TRACE(args...)     do { } while(0)
#define WINE_WARN(args...)      do { } while(0)
#define WINE_ERR(args...)       do { } while(0)
#endif

#define VARLIST_SPISPORT 0
#define VARLIST_SPIMUL 1
#define VARLIST_SPICLOCK 3
#define VARLIST_SPICMDBITS 4
#define VARLIST_SPICMDREADBITS 5
#define VARLIST_SPICMDWRITEBITS 6
#define VARLIST_SPIMAXCLOCK 7
#define VARLIST_FTDI_BASE_CLOCK 8
#define VARLIST_FTDI_LOG_LEVEL 9
#define VARLIST_FTDI_LOG_FILE 10

const SPIVARDEF g_pVarList[]={
	{"SPIPORT","1",1},
	{"SPIMUL","0",0},
	{"SPISHIFTPERIOD","0",0},   /* Unused */
	{"SPICLOCK","0",0},
	{"SPICMDBITS","0",0},
	{"SPICMDREADBITS","0",0},
	{"SPICMDWRITEBITS","0",0},
	{"SPIMAXCLOCK","1000",0},
	{"FTDI_BASE_CLOCK","4000000",0},
	{"FTDI_LOG_LEVEL","warn",0},
	{"FTDI_LOG_FILE","stderr",0}
};

unsigned int g_nSpiMulChipNum=-1;
int g_nSpiPort=1;
unsigned char g_bCurrentOutput=0x10;
unsigned int g_nSpiShiftPeriod=1;
char g_szErrorString[256]="No error";
unsigned int g_nError=SPIERR_NO_ERROR;
int g_nCmdReadBits=0;
int g_nCmdWriteBits=0;
int g_nSpiMul=0;
int g_nSpiMulConfig=0;
unsigned short g_nErrorAddress=0;
spifns_debug_callback g_pDebugCallback=0;

#define BV_MOSI 0x40
#define BV_CSB 0x01
#define BV_MUL 0x02
#define BV_CLK 0x80
#define BV_MISO 0x40 //NOTE: Use status register instead of data

void spifns_debugout(const char *szFormat, ...);
void spifns_debugout_readwrite(unsigned short nAddress, char cOperation, unsigned short nLength, unsigned short *pnData);
int spifns_sequence_setvar(const char *szName, const char *szValue);

//RE Check: Completely identical.
void spifns_getvarlist(const SPIVARDEF **ppList, unsigned int *pnCount) {
    LOG(DEBUG, "");

	*ppList=g_pVarList;
	*pnCount=sizeof(g_pVarList)/sizeof(*g_pVarList);
}

/* Try to initialize variables from environment earlier than pttransport
 * will send them to us */
int spifns_init_vars_from_env(void)
{
    const char *var, *val;
    unsigned int ii;

    for (ii = 0; ii < sizeof(g_pVarList)/sizeof(g_pVarList[0]); ii++) {
        var = g_pVarList[ii].szName;
        val = getenv(var);
        if (val != NULL && val[0] != '\0') {
            if (spifns_sequence_setvar(var, val) !=0)
                return -1;
        }
    }
    return 0;
}

int spifns_init() {
    LOG(DEBUG, "");
    if (spifns_init_vars_from_env() < 0)
        return -1;
    if (spi_init() < 0)
        return -1;
	return 0;
}
//RE Check: Completely identical (one opcode off, jns should be jge, but shouldn't matter. Probably unsigned/signed issues)
const char * spifns_getvar(const char *szName) {
    LOG(DEBUG, "(%s)", szName);
	if (!szName) {
		return "";
	} else if (_stricmp(szName,"SPIPORT")==0) {
		static char szReturn[20];
		sprintf(szReturn,"%d",g_nSpiPort);
		return szReturn;
	} else if (_stricmp(szName,"SPIMUL")==0) {
		static char szReturn[20];
		sprintf(szReturn,"%d",g_nSpiMulChipNum);
		return szReturn;
	} else if (_stricmp(szName,"SPISHIFTPERIOD")==0) {
		static char szReturn[20];
		sprintf(szReturn,"%d",g_nSpiShiftPeriod);
		return szReturn;
	} else if (_stricmp(szName,"SPICLOCK")==0) {
		static char szReturn[64];
		sprintf(szReturn,"%lu",spi_get_clock());
		return szReturn;
	} else if (_stricmp(szName,"SPIMAXCLOCK")==0) {
		static char szReturn[24];
		sprintf(szReturn,"%lu",spi_get_max_clock());
		return szReturn;
	} else {
		return "";
	}
}
//RE Check: Completely identical
unsigned int spifns_get_last_error(unsigned short *pnErrorAddress, const char **pszErrorString) {
    LOG(DEBUG, "");
	if (pnErrorAddress)
		*pnErrorAddress=g_nErrorAddress;
	if (pszErrorString)
		*pszErrorString=g_szErrorString;
	return g_nError;
}

void spifns_clear_last_error(void)
{
    LOG(DEBUG, "");
	static const char szError[]="No error";
	memcpy(g_szErrorString,szError,sizeof(szError));
    g_nErrorAddress=0;
    g_nError=SPIERR_NO_ERROR;
}

//RE Check: Completely identical
void spifns_set_debug_callback(spifns_debug_callback pCallback) {
    LOG(DEBUG, "");
	g_pDebugCallback=pCallback;
    spi_set_error_cb((spi_error_cb)pCallback);
}
//RE Check: Completely identical
int spifns_get_version() {
    LOG(DEBUG, "returning 0x%02x", SPIFNS_VERSION);
	return SPIFNS_VERSION;
}

HANDLE spifns_open_port(int nPort) {
    LOG(INFO, "csr-spi-ftdi %s", VERSION);

    LOG(DEBUG, "(%d)", nPort);

    if (spi_open(nPort - 1) < 0)
        return INVALID_HANDLE_VALUE;

    /* Return some dummy handle value */
    return (HANDLE)&spifns_open_port;
}

void spifns_close_port() {
    LOG(DEBUG, "");
    spi_close();
}
//RE Check: Completely identical
void spifns_debugout(const char *szFormat, ...) {
    LOG(DEBUG, "");
	if (g_pDebugCallback) {
		static char szDebugOutput[256];
		va_list args;
		va_start(args,szFormat);
		vsprintf(szDebugOutput,szFormat,args);
		g_pDebugCallback(szDebugOutput);
		va_end(args);
	}
}

void spifns_close() {
    LOG(DEBUG, "");
	spifns_close_port();

    spi_deinit();
}
//RE Check: Completely identical
void spifns_chip_select(int nChip) {
    LOG(DEBUG, "(%d)", nChip);
	/*g_bCurrentOutput=g_bCurrentOutput&0xD3|0x10;
	g_nSpiMul=0;
	g_nSpiMulConfig=nChip;
	g_nSpiMulChipNum=nChip;
	spifns_debugout(
		"ChipSelect: %04x mul:%d confog:%d chip_num:%d\n",
		g_bCurrentOutput,
		0,
		nChip,
		nChip);*/
}

const char* spifns_command(const char *szCmd) {
	if (stricmp(szCmd,"SPISLOWER")==0) {
        if (spi_clock_slowdown() < 0) {
            /* XXX */
            return 0;
        }
	}
	return 0;
}

void spifns_enumerate_ports(spifns_enumerate_ports_callback pCallback, void *pData) {
    char port_desc[128];
    int nport;

    LOG(DEBUG, "");

    if (spi_nports == 0) {
        /* Some apps, CSR86XX ROM ConfigTool 3.0.48 in particular, crash when
         * no ports present. Return some dummy port for it if we can't find
         * any. */
        LOG(WARN, "No FTDI device found, calling port enum callback "
                "(1, \"No FTDI device found\", %p)", pData);
        pCallback(1, "No FTDI device found", pData);
        return;
    }

    for (nport = 0; nport < spi_nports; nport++) {
        snprintf(port_desc, sizeof(port_desc), "%d: %s %s",
            nport + 1, spi_ports[nport].desc, spi_ports[nport].serial);
        LOG(DEBUG, "Calling port enum callback (%d, \"%s\", %p)",
                nport + 1, port_desc, pData);
        /* Ports start with 1 in spilpt */
        pCallback(nport + 1, port_desc, pData);
    }
}

bool spifns_sequence_setvar_spiport(int nPort) {
    LOG(DEBUG, "(%d)", nPort);

	spifns_close_port();
    if (spifns_open_port(nPort) == INVALID_HANDLE_VALUE)
		return false;
	g_nSpiPort=nPort;
	//TODO: Do this properly!
	g_nSpiShiftPeriod=1;
	return true;
}
//RE Check: Functionally equivalent, but completely different ASM code 
void spifns_debugout_readwrite(unsigned short nAddress, char cOperation, unsigned short nLength, unsigned short *pnData) {
    LOG(DEBUG, "(0x%04x, '%c', %d, buf)", nAddress, cOperation, nLength);
	if (g_pDebugCallback) {
		static const char * const pszTable[]={
			"%04X     %c ????\n",
			"%04X     %c %04X\n",
			"%04X-%04X%c %04X %04X\n",
			"%04X-%04X%c %04X %04X %04X\n",
			"%04X-%04X%c %04X %04X %04X %04X\n",
			"%04X-%04X%c %04X %04X %04X %04X %04X\n",
			"%04X-%04X%c %04X %04X %04X %04X %04X %04X\n",
			"%04X-%04X%c %04X %04X %04X %04X %04X %04X %04X\n",
			"%04X-%04X%c %04X %04X %04X %04X %04X %04X %04X %04X\n",
			"%04X-%04X%c %04X %04X %04X %04X %04X %04X %04X %04X ...\n"
		};
		unsigned short bCopy[8];
#define _MIN(x, y)  ( ((x) < (y)) ? (x) : (y) )
		if (pnData)
			memcpy(bCopy,pnData,sizeof(unsigned short)*_MIN(nLength,8));
		else
			memset(bCopy,0,sizeof(bCopy));
		if (nLength<2) {
            LOG(DEBUG, pszTable[nLength],nAddress,cOperation,bCopy[0]);
			spifns_debugout(pszTable[nLength],nAddress,cOperation,bCopy[0]);
		} else {
            LOG(DEBUG, pszTable[_MIN(nLength, 9)],nAddress,nAddress+nLength-1,cOperation,bCopy[0],bCopy[1],bCopy[2],bCopy[3],bCopy[4],bCopy[5],bCopy[6],bCopy[7]);
			spifns_debugout(pszTable[_MIN(nLength, 9)],nAddress,nAddress+nLength-1,cOperation,bCopy[0],bCopy[1],bCopy[2],bCopy[3],bCopy[4],bCopy[5],bCopy[6],bCopy[7]);
		}
#undef _MIN
	}
}

/* Reimplemented using our SPI impl. */
int spifns_sequence_write(unsigned short nAddress, unsigned short nLength, unsigned short *pnInput) {
    uint8_t outbuf1[] = {
        0x02,                       /* Command: write */
        (uint8_t)(nAddress >> 8),   /* Address high byte */
        (uint8_t)(nAddress & 0xff), /* Address low byte */
    };

    LOG(DEBUG, "(0x%04x, %d, %p)", nAddress, nLength, pnInput);

#define _ERR_RETURN(n, s) do { \
        g_nError = (n); \
        lstrcpynA(g_szErrorString, (s), sizeof(g_szErrorString)); \
        goto error; \
    } while (0)

    if (!spi_isopen())
        _ERR_RETURN(SPIERR_NO_LPT_PORT_SELECTED, "No FTDI device selected");

    DUMP(pnInput, nLength << 1, "write16(addr=0x%04x, len16=%d)",
            nAddress, nLength);

    spi_led(SPI_LED_WRITE);

    if (spi_xfer_begin() < 0)
        _ERR_RETURN(SPIERR_READ_FAILED, "Unable to begin transfer");

    if (spi_xfer_8(SPI_XFER_WRITE, outbuf1, 3) < 0)
        _ERR_RETURN(SPIERR_READ_FAILED, "Unable to start write");

    if (spi_xfer_16(SPI_XFER_WRITE, pnInput, nLength) < 0) {
        spifns_debugout_readwrite(nAddress,'w',nLength, pnInput);
        _ERR_RETURN(SPIERR_READ_FAILED, "Unable to write (writing buffer)");
    }

    if (spi_xfer_end() < 0)
        _ERR_RETURN(SPIERR_READ_FAILED, "Unable to end transfer");

    return 0;

#undef _ERR_RETURN

error:
    if (g_nError != SPIERR_NO_ERROR) {
        g_nErrorAddress=nAddress;
        LOG(ERR, "%s", g_szErrorString);
    }
    return 1;
}

//RE Check: Functionally identical, register choice, calling convention, and some ordering changes.
void spifns_sequence_setvar_spimul(unsigned int nMul) {
    LOG(DEBUG, "(%d)", nMul);
/*	BYTE bNewOutput=g_bCurrentOutput&~BV_CLK;
	if ((g_nSpiMulChipNum=nMul)<=16) {
		//Left side
		if (g_nSpiMulConfig != nMul || g_nSpiMul != 1)
			g_nSpiShiftPeriod=1;
		g_nSpiMulConfig=nMul;
		g_nSpiMul=1;
		bNewOutput=(bNewOutput&(BV_CLK|BV_MOSI|BV_CSB))|((BYTE)nMul<<1);
	} else {
		//Right side
		//loc_10001995
		if (g_nSpiMulConfig != 0 || g_nSpiMul!=0)
			g_nSpiShiftPeriod=1;
		g_nSpiMul=0;
		g_nSpiMulConfig=0;
		bNewOutput&=BV_CLK|BV_MOSI|BV_CSB;
	}
	g_bCurrentOutput=bNewOutput;
	SPITransfer(0,2,0,0);
	LARGE_INTEGER liFrequency,liPCStart,liPCEnd;
	QueryPerformanceFrequency(&liFrequency);
	QueryPerformanceCounter(&liPCStart);
	do {
		Sleep(0);
		QueryPerformanceCounter(&liPCEnd);
	} while (1000 * (liPCEnd.QuadPart - liPCStart.QuadPart) / liFrequency.QuadPart < 5);
	spifns_debugout("MulitplexSelect: %04x mul:%d config:%d chip_num:%d\n",g_bCurrentOutput,g_nSpiMul,g_nSpiMulConfig,g_nSpiMulChipNum);*/
}
//RE Check: Functionally identical, register choice, calling convention, stack size, and some ordering changes.
int spifns_sequence_setvar(const char *szName, const char *szValue) {
    LOG(DEBUG, "(%s, %s)", szName, szValue);
	if (szName==0)
		return 1;
	if (szValue==0)
		return 1;
	long nValue=strtol(szValue,0,0);
	for (unsigned int i=0; i<(sizeof(g_pVarList)/sizeof(*g_pVarList)); i++) {
		if (stricmp(szName,g_pVarList[i].szName)==0) {
			switch (i) {
			case VARLIST_SPISPORT:{
				if (!spifns_sequence_setvar_spiport(nValue)) {
					const char szError[]="Couldn't find SPI port";
					memcpy(g_szErrorString,szError,sizeof(szError));
					return 1;
				}
								  }break;
			case VARLIST_SPIMUL:{
				spifns_sequence_setvar_spimul(nValue);
								}break;
			case VARLIST_SPICLOCK:
				if (nValue <= 0)
					return 1; //ERROR!
                if (spi_set_clock((unsigned long)nValue) < 0) {
                    const char szError[]="Couldn't set SPI clock";
                    memcpy(g_szErrorString,szError,sizeof(szError));
                    return 1;
                }
				break;
			case VARLIST_SPICMDBITS:
			case VARLIST_SPICMDREADBITS:
			case VARLIST_SPICMDWRITEBITS:{
				if (i!=VARLIST_SPICMDREADBITS)
					g_nCmdWriteBits=nValue;
				if (i!=VARLIST_SPICMDWRITEBITS)
					g_nCmdReadBits=nValue;
										 }break;
			case VARLIST_SPIMAXCLOCK:
                if (nValue <= 0) {
                    const char szError[]="SPIMAXCLOCK value should be positive integer";
                    memcpy(g_szErrorString,szError,sizeof(szError));
                    return 1;
                }
                spi_set_max_clock((unsigned long)nValue);
				break;

            case VARLIST_FTDI_BASE_CLOCK:
                if (nValue <= 0) {
                    const char szError[]="FTDI_BASE_CLOCK value should be positive integer";
                    memcpy(g_szErrorString,szError,sizeof(szError));
                    return 1;
                }
                spi_set_ftdi_base_clock((unsigned long)nValue);
                break;
            case VARLIST_FTDI_LOG_LEVEL:
                {
                    char *val, *cp, *tok;
                    uint32_t lvl;

                    val = strdup(szValue);
                    if (val == NULL)
                        return 1;

                    cp = val;
                    lvl = 0;
                    while ((tok = strtok(cp, ",")) != NULL) {
                        cp = NULL;
                        switch (toupper(tok[0])) {
                        case 'Q':
                            lvl = (lvl & ~LOG_LEVEL_MASK) | LOG_LEVEL_QUIET;
                            break;
                        case 'E':
                            lvl = (lvl & ~LOG_LEVEL_MASK) | LOG_LEVEL_ERR;
                            break;
                        case 'W':
                            lvl = (lvl & ~LOG_LEVEL_MASK) | LOG_LEVEL_WARN;
                            break;
                        case 'I':
                            lvl = (lvl & ~LOG_LEVEL_MASK) | LOG_LEVEL_INFO;
                            break;
                        case 'D':
                            if (toupper(tok[1]) == 'E') /* DEBUG */
                                lvl = (lvl & ~LOG_LEVEL_MASK) | LOG_LEVEL_DEBUG;
                            else    /* DUMP */
                                lvl |= LOG_FLAGS_DUMP;
                            break;
                        default:
                            free(val);
                            return 1;
                        }
                    }

                    free(val);
                    log_set_options(lvl);
                }
                break;
            case VARLIST_FTDI_LOG_FILE:
                if (!strcasecmp(szValue, "stdout")) {
                    log_set_dest(stdout);
                } else if (!strcasecmp(szValue, "stderr")) {
                    log_set_dest(stderr);
                } else {
                    if (log_set_file(szValue) < 0) {
                        const char szError[]="Couldn't open log file";
                        memcpy(g_szErrorString,szError,sizeof(szError));
                        return 1;
                    }
                }
                break;
			}
		}
	}
	return 0;
}

/* Reimplemented using our SPI impl. */
int spifns_sequence_read(unsigned short nAddress, unsigned short nLength, unsigned short *pnOutput) {
    uint8_t outbuf[] = {
        3,                          /* Command: read */
        (uint8_t)(nAddress >> 8),   /* Address high byte */
        (uint8_t)(nAddress & 0xff), /* Address low byte */
    };
    uint8_t inbuf1[2];

    LOG(DEBUG, "(0x%02x, %d, %p)", nAddress, nLength, pnOutput);

#define _ERR_RETURN(n, s) do { \
        g_nError = (n); \
        lstrcpynA(g_szErrorString, (s), sizeof(g_szErrorString)); \
        goto error; \
    } while (0)

    if (!spi_isopen())
        _ERR_RETURN(SPIERR_NO_LPT_PORT_SELECTED, "No FTDI device selected");

    spi_led(SPI_LED_READ);

    if (spi_xfer_begin() < 0)
        _ERR_RETURN(SPIERR_READ_FAILED, "Unable to begin transfer");

    if (spi_xfer_8(SPI_XFER_WRITE, outbuf, 3) < 0)
        _ERR_RETURN(SPIERR_READ_FAILED, "Unable to start read");

    if (spi_xfer_8(SPI_XFER_READ, inbuf1, 2) < 0)
        _ERR_RETURN(SPIERR_READ_FAILED,
                "Unable to start read (getting control data)");

    if (inbuf1[0] != 3 || inbuf1[1] != (nAddress >> 8)) {
        _ERR_RETURN(SPIERR_READ_FAILED,
                "Unable to start read (invalid control data)");
        LOG(ERR, "Control data: 0x%02x 0x%02x", inbuf1[0], inbuf1[1]);
    }

    if (spi_xfer_16(SPI_XFER_READ, pnOutput, nLength) < 0) {
        spifns_debugout_readwrite(nAddress,'r', nLength, pnOutput);
        _ERR_RETURN(SPIERR_READ_FAILED, "Unable to read (reading buffer)");
    }

    if (spi_xfer_end() < 0)
        _ERR_RETURN(SPIERR_READ_FAILED, "Unable to end transfer");

    DUMP(pnOutput, nLength << 1, "read16(addr=0x%04x, len16=%d)",
            nAddress, nLength);

	return 0;

#undef _ERR_RETURN

error:
    if (g_nError != SPIERR_NO_ERROR) {
        g_nErrorAddress=nAddress;
        LOG(ERR, "%s", g_szErrorString);
    }
    return 1;
}
//RE Check: Functionally identical, can't get the ASM code to match.
int spifns_sequence(SPISEQ *pSequence, unsigned int nCount) {
	int nRetval=0;

    LOG(DEBUG, "(%p, %d)", pSequence, nCount);

	while (nCount--) {
        LOG(DEBUG, "command %d", pSequence->nType);
		switch (pSequence->nType) {
		case SPISEQ::TYPE_READ:{
			if (spifns_sequence_read(pSequence->rw.nAddress,pSequence->rw.nLength,pSequence->rw.pnData)==1)
				nRetval=1;
							   }break;
		case SPISEQ::TYPE_WRITE:{
			if (spifns_sequence_write(pSequence->rw.nAddress,pSequence->rw.nLength,pSequence->rw.pnData)==1)
				nRetval=1;
								}break;
		case SPISEQ::TYPE_SETVAR:{
			if (spifns_sequence_setvar(pSequence->setvar.szName,pSequence->setvar.szValue)==1)
				nRetval=1;
								 }break;
        default:
            LOG(WARN, "Sequence command not implemented: %d", pSequence->nType);
            g_nError = SPIFNS_ERROR_INVALID_PARAMETER;
            snprintf(g_szErrorString, sizeof(g_szErrorString),
                    "sequence command %d not implemented", pSequence->nType);
            nRetval = 1;

		}
		pSequence++;
	}
	return nRetval;
}

/* Reimplemented using our own SPI driver */
int spifns_bluecore_xap_stopped() {
    /* Read chip version */
    uint8_t xferbuf[] = {
        3,      /* Command: read */
        GBL_CHIP_VERSION_GEN1_ADDR >> 8,   /* Address high byte */
        GBL_CHIP_VERSION_GEN1_ADDR & 0xff,   /* Address low byte */
    };
    uint8_t inbuf[2];

    LOG(DEBUG, "");

    if (spi_xfer_begin() < 0)
        return SPIFNS_XAP_NO_REPLY;
    if (spi_xfer_8(SPI_XFER_READ | SPI_XFER_WRITE, xferbuf, 3) < 0)
        return SPIFNS_XAP_NO_REPLY;
    if (spi_xfer_8(SPI_XFER_READ, inbuf, 2) < 0)
        return SPIFNS_XAP_NO_REPLY;
    if (spi_xfer_end() < 0)
        return SPIFNS_XAP_NO_REPLY;
    DUMP(inbuf, 2, "read8(addr=0x%04x, len=%d)", GBL_CHIP_VERSION_GEN1_ADDR, 3);
    LOG(DEBUG, "CPU is %s", xferbuf[0] ? "stopped" : "running");

    if (inbuf[0] != 3 || inbuf[1] != 0xff) {
        /* No chip present or not responding correctly, no way to find out. */
        return SPIFNS_XAP_NO_REPLY;
    }

    /* Check the response to read command */
    /* From CSR8645 datasheet: "When CSR8645 BGA is deselected (SPI_CS# = 1),
     * the SPI_MISO line does not float. Instead, CSR8645 BGA outputs 0 if the
     * processor is running or 1 if it is stopped. */
    if (xferbuf[0])
        return SPIFNS_XAP_STOPPED;
    return SPIFNS_XAP_RUNNING;
}

#if SPIFNS_API == SPIFNS_API_1_4
/* This is a limited implementation of CSR SPI API 1.4. It supports only 1
 * stream and does not support all of the features. */

int spifns_stream_init(spifns_stream_t *p_stream)
{
    LOG(DEBUG, "(%p)", p_stream);
    int rc;
    
    rc = spifns_init();
    if (rc == 0)
        *p_stream = (spifns_stream_t)0;
    
    return rc;
}

void spifns_stream_close(spifns_stream_t stream)
{
    LOG(DEBUG, "(%d)", stream);
    spifns_close();
}

unsigned int spifns_count_streams(void)
{
    LOG(DEBUG, "");
    return 1;
}

int spifns_stream_sequence(spifns_stream_t stream, SPISEQ_1_4 *pSequence, int nCount)
{
	int nRetval=0;

    LOG(DEBUG, "(%d, %p, %d)", stream, pSequence, nCount);

	while (nCount--) {
        LOG(DEBUG, "command %d", pSequence->nType);
		switch (pSequence->nType) {
		case SPISEQ_1_4::TYPE_READ:
			if (spifns_sequence_read(pSequence->rw.nAddress,pSequence->rw.nLength,pSequence->rw.pnData)==1)
				nRetval=1;
			break;
		case SPISEQ_1_4::TYPE_WRITE:
			if (spifns_sequence_write(pSequence->rw.nAddress,pSequence->rw.nLength,pSequence->rw.pnData)==1)
				nRetval=1;
			break;
		case SPISEQ_1_4::TYPE_SETVAR:
			if (spifns_sequence_setvar(pSequence->setvar.szName,pSequence->setvar.szValue)==1)
				nRetval=1;
			break;
        default:
            LOG(WARN, "Sequence command not implemented: %d", pSequence->nType);
            g_nError = SPIFNS_ERROR_INVALID_PARAMETER;
            snprintf(g_szErrorString, sizeof(g_szErrorString),
                    "sequence command %d not implemented", pSequence->nType);
            nRetval = 1;
		}
		pSequence++;
	}
	return nRetval;
}

const char* spifns_stream_command(spifns_stream_t stream, const char *command)
{
    LOG(DEBUG, "(%d, %s)", stream, command);
    return spifns_command(command);
}

const char* spifns_stream_getvar(spifns_stream_t stream, const char *var)
{
    LOG(DEBUG, "(%d, %s)", stream, var);
    return spifns_getvar(var);
}

void spifns_stream_chip_select(spifns_stream_t stream, int which)
{
    LOG(DEBUG, "(%d, %d)", stream, which);
    spifns_chip_select(which);
}

int spifns_stream_bluecore_xap_stopped(spifns_stream_t stream)
{
    LOG(DEBUG, "(%d)", stream);
    return spifns_bluecore_xap_stopped();
}

/* returns the last error code, and if a pointer is passed in, the problematic
 * address.*/
/* get_last_error and clear_last_error both deal with the error that occurred
 * in the current thread */
int spifns_get_last_error32(uint32_t *addr, const char ** buf)
{
    unsigned short saddr;
    int rc;

    LOG(DEBUG, "(%p, %p)", addr, buf);

    rc = spifns_get_last_error(&saddr, buf);
    if (addr)
        *addr = saddr;
    return rc;
}

void spifns_stream_set_debug_callback(spifns_stream_t stream, spifns_debug_callback fn, void *pvcontext)
{
    LOG(DEBUG, "(%d, %p, %p)", stream, fn, pvcontext);
    spifns_set_debug_callback(fn);
}

int spifns_stream_get_device_id(spifns_stream_t stream, char *buf, size_t length)
{
    LOG(DEBUG, "(%d, %p, %u)", stream, buf, length);
    snprintf(buf, length, "FTDISyncBB");
    return 0;
}

int spifns_stream_lock(spifns_stream_t stream, uint32_t timeout)
{
    LOG(DEBUG, "(%d, %u)", stream, timeout);
    return 0;
}

void spifns_stream_unlock(spifns_stream_t stream)
{
    LOG(DEBUG, "(%d)", stream);
}

#endif /* SPIFNS_API == SPIFNS_API_1_4 */
