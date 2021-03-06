// S982VGM.cpp : Defines the entry point for the console application.
//
#pragma pack(1) // or /Zp1 option VC++ 2017
#define _USE_MATH_DEFINES
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "stdtype.h"
#include "stdbool.h"
#include "VGMFile.h"

enum chip {
	SN76489 = 0, YM2413, YM2612, YM2151, SPCM, RF5C68, YM2203, YM2608,
	YM2610, YM3812, YM3526, Y8950, YMF262, YMF278B, YMF271, YMZ280B,
	RF5C164, PWM, AY8910, GBDMG, NESAPU, MultiPCM, uPD7759, OKIM6258,
	OKIM6295, K051649, K054539, HuC6280, C140, K053260, Pokey, QSound,
	SCSP, WonderSwan, VSU, SAA1099, ES5503, ES5506, X1_010, C352,
	GA20
};
enum psg_type { AY_AY8910 = 0, AY_AY8912, AY_AY8913, AY_AY8930, AY_YM2149 = 0x10, AY_YM3439, AY_YMZ284, AY_YMZ294 };

enum s98_chip { NONE = 0, SSG, OPN, OPN2, OPNA, OPM, OPLL, OPL, OPL2, OPL3, PSG = 15, DCSG};

const char *s98_chipname[] = { "NONE", "YM2149", "YM2203", "YM2612", "YM2608", "YM2151", "YM2413", "YM3526",
							"YM3812", "YMF262", "", "", "", "", "", "AY-3-8910",
							"SN76489" };

const UINT8 s98_vgm_chipmap[] = { 0xFF, AY8910, YM2203, YM2612, YM2608, YM2151, YM2413, YM3526,
							YM3812, YMF262,	0xFF, 0xFF, 0xFF, 0xFF, 0xFF, AY8910,
							SN76489 };

const UINT8 s98_vgm_cmd[] = { 0xFF, 0xA0, 0x55, 0x52, 0x56, 0x54, 0x51, 0x5B,
							0x5A, 0x5E, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xA0,
							0x50 };

const UINT8 s98_vgm_aytype[] = { AY_AY8910, AY_YM2149, AY_AY8910, AY_AY8910, AY_AY8910, AY_AY8910, AY_AY8910, AY_AY8910,
							AY_AY8910, AY_AY8910, AY_AY8910, AY_AY8910, AY_AY8910, AY_AY8910, AY_AY8910, AY_AY8910,
							AY_AY8910 };

typedef struct _s98_devinfo {
	UINT32	Type;
	UINT32	Clock;
	UINT32	Pan;
	UINT32	dummy;
} S98DEVINFO;

typedef struct _s98_header {
	UINT32	fccS98;
	UINT32	resolution_numerator;
	UINT32	resolution_denominator;
	UINT32	compression; // after V3 must be 0 (No implements exist)
	UINT32	offset_tag; // absolute address
	UINT32	offset_dump; // absolute address
	UINT32	offset_loop; // absolute address
	UINT32	NumDev;
} S98HEADER;


typedef struct _s98_vgm_devmap
{
	UINT8 Type;
	UINT8 AY_Type;
	UINT8 Count;
	UINT8 CMD;
	UINT32 Clock;
	UINT32 *ClockAddr;
	UINT32 Pan;
	UINT16 Volume;
} S98DEVMAP;

#define FCC_S98 0x33383953 // "S983"


typedef struct _vol_preset {
	UINT8 master_vol;
	INT32 ssg_vol;
} VOLPRESET;

VGM_HEADER h_vgm = { FCC_VGM, 0, 0x170};

const UINT32 *s98_vgm_clock_addr[] = { NULL, &h_vgm.lngHzAY8910, &h_vgm.lngHzYM2203, &h_vgm.lngHzYM2612,
									&h_vgm.lngHzYM2608, &h_vgm.lngHzYM2151, &h_vgm.lngHzYM2413, &h_vgm.lngHzYM3526,
									&h_vgm.lngHzYM3812, &h_vgm.lngHzYMF262, NULL, NULL,
									NULL, NULL, NULL, &h_vgm.lngHzAY8910,
									&h_vgm.lngHzPSG };
int main(int argc, char **argv)
{
	if (argc < 2) {
		fprintf_s(stderr, "Usage: %s file", *argv);
		exit(-1);
	}

	FILE *pFi;
	errno_t rc;

	rc = fopen_s(&pFi, *++argv, "rb");

	if (rc) {
		fprintf_s(stderr, "%s cannot open\n", *argv);
		exit(rc);
	}


	if (argc > 2) {
		INT32 vol;
		vol = atoi(*(argv + 1));
		if (vol < 25)
			vol = 25;
		else if (vol > 640) {
			vol = 640;
		}
		double vol_log = 32.0 * log(((double)vol / 100.0)) / M_LN2;
		INT32 vol_int = (INT32)(vol_log);

		if (vol_int <= -64)
			vol_int = -63;
		else if (vol_int > 192) {
			vol_int = 192;
		}

		h_vgm.bytVolumeModifier = (UINT8)(vol_int & 0xFF);
	}

	INT32 ssg_level = 0;
	if (argc > 3) {
		ssg_level = atoi(*(argv + 2));
	}

	S98HEADER h_s98;
	size_t rsize;

	rsize = fread_s(&h_s98, sizeof(S98HEADER), 1, sizeof(S98HEADER), pFi);

	if (rsize != sizeof(S98HEADER)) {
		fprintf_s(stderr, "%s cannot read\n", *argv);
		exit(-2);
	}

	if (h_s98.fccS98 != FCC_S98) {
		fprintf_s(stderr, "%s is not VGM %08X %08X\n", *argv, h_s98.fccS98, FCC_S98);
		exit(-10);
	}

	S98DEVINFO(*devinfo)[] = NULL;
	UINT8 s98_chipcount[17] = { 0 };
	S98DEVMAP(*devmap)[] = NULL;
	UINT32 NumDev = h_s98.NumDev;
	if (NumDev) {
		if (NULL == (devinfo = (S98DEVINFO(*)[])malloc(sizeof(S98DEVINFO)*NumDev))) {
			fprintf_s(stderr, "memory allocation failed\n");
			exit(errno);
		}
		rsize = fread_s(devinfo, sizeof(S98DEVINFO)*NumDev, sizeof(S98DEVINFO), NumDev, pFi);

		if (rsize != NumDev) {
			fprintf_s(stderr, "%s cannot read\n", *argv);
			exit(-2);
		}
		if (NULL == (devmap = (S98DEVMAP(*)[])malloc(sizeof(S98DEVMAP)*NumDev))) {
			fprintf_s(stderr, "memory allocation failed\n");
			exit(errno);
		}
		for (UINT32 i = 0; i < NumDev; i++) {
			(*devmap)[i].Type = s98_vgm_chipmap[(*devinfo)[i].Type];
			(*devmap)[i].AY_Type = s98_vgm_aytype[(*devinfo)[i].Type];
			(*devmap)[i].CMD = s98_vgm_cmd[(*devinfo)[i].Type];
			(*devmap)[i].Clock = (*devinfo)[i].Clock;
			(*devmap)[i].Pan = (*devinfo)[i].Pan;
			(*devmap)[i].Count = ++s98_chipcount[(*devinfo)[i].Type];
			(*devmap)[i].ClockAddr = s98_vgm_clock_addr[(*devinfo)[i].Type];
		}
	}
	else {
		NumDev = 1;
		if (NULL == (devmap = (S98DEVMAP(*)[])malloc(sizeof(S98DEVMAP)))) {
			fprintf_s(stderr, "memory allocation failed\n");
			exit(errno);
		}
		(*devmap)[0].Type = s98_vgm_chipmap[OPNA];
		(*devmap)[0].CMD = s98_vgm_cmd[OPNA];
		(*devmap)[0].Clock = 7987200;
		(*devmap)[0].Count = ++s98_chipcount[OPNA];
		(*devmap)[0].ClockAddr = s98_vgm_clock_addr[OPNA];
	}

	printf_s("%2d chip(s) found\n", NumDev);
	UINT8 Ex_ClockCount = 0;
	VGMX_CHIP_DATA32 Ex_Clocks[10];
	UINT8 Ex_VolCount = 0;
	VGMX_CHIP_DATA16 Ex_Vols[20];

	for (UINT32 i = 0; i < NumDev; i++) {
		printf_s("Chip:%2d_%2d %10dHz %2d\n", (*devmap)[i].Type, (*devmap)[i].AY_Type, (*devmap)[i].Clock, (*devmap)[i].Count);
		if ((*devmap)[i].Count == 1) {
			switch ((*devmap)[i].Type) {
			case AY8910:
				*(*devmap)[i].ClockAddr = (*devmap)[i].Clock;
				h_vgm.bytAYType = (*devmap)[i].AY_Type;
				h_vgm.bytAYFlag = 0x1;
				Ex_Vols[Ex_VolCount].Type = (*devmap)[i].Type;
				Ex_Vols[Ex_VolCount].Flags = 0;
				Ex_Vols[Ex_VolCount].Data = 0x8100;
				Ex_VolCount++;
				break;
			case YM2203:
			case YM2608:
				*(*devmap)[i].ClockAddr = (*devmap)[i].Clock;
				Ex_Vols[Ex_VolCount].Type = (*devmap)[i].Type | 0x80;
				Ex_Vols[Ex_VolCount].Flags = 0;
				Ex_Vols[Ex_VolCount].Data = 0x8100;
				Ex_VolCount++;
				break;
			case YM2612:
			case YM2151:
			case YM2413:
			case YM3526:
			case YM3812:
			case YMF262:
			case SN76489:
				*(*devmap)[i].ClockAddr = (*devmap)[i].Clock;
				break;
			}
		}
		else if ((*devmap)[i].Count == 2) { // dual chips
			switch ((*devmap)[i].Type) {
			case AY8910:
				if (*(*devmap)[i].ClockAddr != (*devmap)[i].Clock) {
					Ex_Clocks[Ex_ClockCount].Type = (*devmap)[i].Type;
					Ex_Clocks[Ex_ClockCount].Data = (*devmap)[i].Clock;
					Ex_ClockCount++;
					Ex_Vols[Ex_VolCount].Type = (*devmap)[i].Type;
					Ex_Vols[Ex_VolCount].Flags = 1;
					Ex_Vols[Ex_VolCount].Data = 0x8100;
					Ex_VolCount++;
					printf_s("2nd chip clock not equal to 1st.\n");
				}
				if (h_vgm.bytAYType != (*devmap)[i].AY_Type) {
					printf_s("2nd chip type not match to 1st.\n");
				}
				*(*devmap)[i].ClockAddr |= 0x40000000;
				break;
			case YM2203:
			case YM2608:
				if (*(*devmap)[i].ClockAddr != (*devmap)[i].Clock) {
					Ex_Clocks[Ex_ClockCount].Type = (*devmap)[i].Type;
					Ex_Clocks[Ex_ClockCount].Data = (*devmap)[i].Clock;
					Ex_ClockCount++;
					Ex_Vols[Ex_VolCount].Type = (*devmap)[i].Type | 0x80;
					Ex_Vols[Ex_VolCount].Flags = 1;
					Ex_Vols[Ex_VolCount].Data = 0x8100;
					Ex_VolCount++;
					printf_s("2nd chip clock not equal to 1st.\n");
				}
				*(*devmap)[i].ClockAddr |= 0x40000000;
				(*devmap)[i].CMD ^= 0xF0;
				break;
			case YM2612:
			case YM2151:
			case YM2413:
			case YM3526:
			case YM3812:
				if (*(*devmap)[i].ClockAddr != (*devmap)[i].Clock) {
					Ex_Clocks[Ex_ClockCount].Type = (*devmap)[i].Type;
					Ex_Clocks[Ex_ClockCount].Data = (*devmap)[i].Clock;
					Ex_ClockCount++;
					printf_s("2nd chip clock not equal to 1st.\n");
				}
				*(*devmap)[i].ClockAddr |= 0x40000000;
				(*devmap)[i].CMD ^= 0xF0;
				break;
			case YMF262:
				if (*(*devmap)[i].ClockAddr != (*devmap)[i].Clock) {
					Ex_Clocks[Ex_ClockCount].Type = (*devmap)[i].Type;
					Ex_Clocks[Ex_ClockCount].Data = (*devmap)[i].Clock;
					Ex_ClockCount++;
					printf_s("2nd chip clock not equal to 1st.\n");
				}
				*(*devmap)[i].ClockAddr |= 0x40000000;
				(*devmap)[i].CMD ^= 0xF0;
				break;
			case SN76489:
				if (*(*devmap)[i].ClockAddr != (*devmap)[i].Clock) {
					Ex_Clocks[Ex_ClockCount].Type = (*devmap)[i].Type;
					Ex_Clocks[Ex_ClockCount].Data = (*devmap)[i].Clock;
					Ex_ClockCount++;
					printf_s("2nd chip clock not equal to 1st.\n");
				}
				*(*devmap)[i].ClockAddr |= 0x40000000;
				(*devmap)[i].CMD = 0x30;
				break;
			}
		}
		else if ((*devmap)[i].Count >= 3) { // triple or more chips
			printf_s("Oh, NO! I cannot support over 3 chips\n");
			exit(-3);
		}
	}
	for (UINT32 i = 0; i < 17; i++) {
		printf_s("%2d ", s98_chipcount[i]);
	}
	printf_s("\n");

	if (h_vgm.lngHzYM2151 == 4000000) {
		if (h_vgm.lngHzYM2151 == h_vgm.lngHzAY8910) {
			if (h_vgm.bytAYType == AY_YM2149) {
				// If VGM Specification became 1.71+
				// It must be replaced to AY Flags code
				// h_vgm.bytAYFlags |= 0x10;
				h_vgm.lngHzAY8910 >>= 1;
				printf_s("X1turbo patch applyed!\n");
			}
		}
	}

	struct _stat st;
	if (_fstat(_fileno(pFi), &st) < 0) {
		fprintf_s(stderr, "%s status wrong\n", *argv);
		exit(errno);
	}

	size_t s98_dlen = st.st_size - h_s98.offset_dump;
	UINT8 *s98_data;
	printf("data length = %zu bytes\n", s98_dlen);

	if (NULL == (s98_data = (UINT8 *)malloc(s98_dlen))) {
		fprintf_s(stderr, "memory allocation failed\n");
		exit(errno);
	}
	rsize = fread_s(s98_data, s98_dlen, 1, s98_dlen, pFi);

	if (rsize != s98_dlen) {
		fprintf_s(stderr, "%s cannot read\n", *argv);
		exit(-2);
	}

	size_t vgm_buflen = s98_dlen * 6;
	UINT8 *vgm_data, *vgm_pos;

	if (NULL == (vgm_data = (UINT8 *)malloc(vgm_buflen))) {
		fprintf_s(stderr, "memory allocation failed\n");
		exit(errno);
	}

	vgm_pos = vgm_data;

	UINT32 total_samples = 0;
	UINT32 delta_samples = 0, new_delta_sum = 0;
	UINT32 new_total;


	double sample_factor_2 = 2.0 * 44100.0 * (double)h_s98.resolution_numerator / (double)h_s98.resolution_denominator;

	// converting section

	do {
		int s, n;
		if (*s98_data == 0xFD) {
#define FIXLEN1 882
#define FIXLEN2 735
#define FIXLEN3 16

#define PUT_SAMPLES \
			if (vgm_pos == vgm_data) {delta_samples = 0;} \
			else if (delta_samples) { \
				\
				total_samples += delta_samples;	delta_samples = 0; \
				new_total = (UINT32)((double)total_samples * sample_factor_2); new_total++;	new_total >>= 1; \
				UINT32 new_delta = new_total - new_delta_sum; new_delta_sum += new_delta; \
				while (new_delta) { \
					if (new_delta > 0xFFFF) {*vgm_pos++ = 0x61; *vgm_pos++ = 0xFF; *vgm_pos++ = 0xFF; new_delta -= 0xFFFF;} \
					else if (new_delta == FIXLEN1 * 2 || new_delta == FIXLEN1 + FIXLEN2 || (new_delta <= FIXLEN1 + FIXLEN3 && new_delta >= FIXLEN1)) {*vgm_pos++ = 0x63; new_delta -= FIXLEN1;} \
					else if (new_delta == FIXLEN2 * 2 || (new_delta <= FIXLEN2 + FIXLEN3 && new_delta >= FIXLEN2)) {*vgm_pos++ = 0x62; new_delta -= FIXLEN2;} \
					else if (new_delta <= FIXLEN3 * 2 && new_delta >= FIXLEN3) {*vgm_pos++ = 0x7F; new_delta -= FIXLEN3;} \
					else if (new_delta <= 15) {*vgm_pos++ = 0x70 | (new_delta - 1); new_delta = 0;} \
					else {*vgm_pos++ = 0x61; *vgm_pos++ = new_delta & 0xFF; *vgm_pos++ = (new_delta >> 8) & 0xFF; new_delta = 0;} \
				} \
			}


			// PUT_SAMPLES  // skip the last wait.
			*vgm_pos++ = 0x66;
			h_vgm.lngTotalSamples = new_total;
			s98_data++;
			s98_dlen--;
			break;
		}
		else if (*s98_data == 0xFF) {
			s98_data++;
			s98_dlen--;
			delta_samples++;
		}
		else if (*s98_data == 0xFE) {
			s98_data++;
			s98_dlen--;
			s = 0;
			n = 0;
			do
			{
				n |= (*s98_data & 0x7f) << s;
				s += 7;
				s98_dlen--;
			} while (*(s98_data++) & 0x80);
			delta_samples += (n + 2);
		}
		else if (*s98_data < NumDev * 2) {
			size_t index = *s98_data >> 1;
			UINT8 extra = *s98_data & 1;

			PUT_SAMPLES

			if (extra && ((*devmap)[index].Type == YM2608 || (*devmap)[index].Type == YM2612 || (*devmap)[index].Type == YMF262)) {
					*vgm_pos++ = (*devmap)[index].CMD | 0x1;
					*vgm_pos++ = *(s98_data + 1);
					*vgm_pos++ = *(s98_data + 2);
			}
			else if ((*devmap)[index].Type == AY8910 && (*devmap)[index].Count == 2) {
					*vgm_pos++ = (*devmap)[index].CMD;
					*vgm_pos++ = *(s98_data + 1) | 0x80;
					*vgm_pos++ = *(s98_data + 2);
			}
			else if ((*devmap)[index].Type == SN76489) {
					*vgm_pos++ = (*devmap)[index].CMD;
					*vgm_pos++ = *(s98_data + 2);
			}
			else {
					*vgm_pos++ = (*devmap)[index].CMD;
					*vgm_pos++ = *(s98_data + 1);
					*vgm_pos++ = *(s98_data + 2);
			}

			s98_data += 3;
			s98_dlen -= 3;
		}
	} while (1);

	size_t vgm_dlen = vgm_pos - vgm_data;
	printf_s("vgm data length %zu bytes\n", vgm_dlen);

	VGM_HDR_EXTRA eh_vgm = { sizeof(VGM_HDR_EXTRA), 0, 0 };

	for (size_t i = 0; i < Ex_VolCount; i++) {
		if (Ex_Vols[i].Type == AY8910 || Ex_Vols[i].Type == (YM2203 | 0x80) || Ex_Vols[i].Type == (YM2608 | 0x80)) {
			if (ssg_level > 0) {
				double ssg_factor = (double)ssg_level * 256.0 * 2.0/ 100.0;
				if (ssg_factor > 32767)
					ssg_factor = 32767.0;
				UINT16 ssg_vol = (UINT16)ssg_factor;
				printf("new SSG vol data: %04X\n", ssg_vol);
				Ex_Vols[i].Data = ssg_vol | 0x8000;
			}
		}
	}

	size_t Ex_ClockSize = sizeof(VGMX_CHIP_DATA32) * Ex_ClockCount;
	size_t Ex_VolSize = sizeof(VGMX_CHIP_DATA16) * Ex_VolCount;

	if (Ex_ClockSize) {
		eh_vgm.Chp2ClkOffset = sizeof(UINT32) * 2;
	}
	if (Ex_VolSize) {
		eh_vgm.ChpVolOffset = sizeof(UINT32);
		if (Ex_ClockSize) {
			eh_vgm.ChpVolOffset += 1 + Ex_ClockSize;
		}
	}

	printf("Extra Clock/Vol size = %d/%d\nExtra Clock/Vol offs = %d/%d\n", Ex_ClockSize, Ex_VolSize, eh_vgm.Chp2ClkOffset, eh_vgm.ChpVolOffset);

	size_t vgm_extra_len;
	if (Ex_ClockSize + Ex_VolSize == 0) {
		vgm_extra_len = 0;
	}
	else {
		vgm_extra_len = sizeof(VGM_HDR_EXTRA);
		if (Ex_ClockSize) {
			vgm_extra_len += Ex_ClockSize + 1;
		}
		if (Ex_VolSize) {
			vgm_extra_len += Ex_VolSize + 1;
		}

	}
	printf("Extra Header length %d\n", vgm_extra_len);
	size_t padsize = (~vgm_extra_len + 1) & 0xF;
	vgm_extra_len += padsize;
	printf("New Extra Header length %d\n", vgm_extra_len);

	size_t vgm_header_len = 0xc0;
	size_t vgm_data_abs = vgm_header_len + vgm_extra_len;
	h_vgm.lngDataOffset = vgm_data_abs - ((UINT8 *)&h_vgm.lngDataOffset - (UINT8 *)&h_vgm.fccVGM);
	h_vgm.lngExtraOffset = vgm_header_len - ((UINT8 *)&h_vgm.lngExtraOffset - (UINT8 *)&h_vgm.fccVGM);
	h_vgm.lngEOFOffset = vgm_data_abs + vgm_dlen - ((UINT8 *)&h_vgm.lngEOFOffset - (UINT8 *)&h_vgm.fccVGM);

	FILE *pFo;
	char fn[_MAX_FNAME], fpath[_MAX_PATH];

	_splitpath_s(*argv, NULL, 0, NULL, 0, fn, _MAX_FNAME, NULL, 0);
	_makepath_s(fpath, _MAX_PATH, NULL, NULL, fn, ".vgm");

	rc = fopen_s(&pFo, fpath, "wb");
	if (rc) {
		fprintf_s(stderr, "%s cannot open\n", fpath);
		exit(rc);
	}

	fwrite(&h_vgm, 1, vgm_header_len, pFo);

	if (vgm_extra_len) {
		fwrite(&eh_vgm, 1, sizeof(VGM_HDR_EXTRA), pFo);
		if (Ex_ClockCount) {
			fwrite(&Ex_ClockCount, 1, 1, pFo);
			fwrite(&Ex_Clocks, sizeof(VGMX_CHIP_DATA32), Ex_ClockCount, pFo);
		}
		if (Ex_VolCount) {
			fwrite(&Ex_VolCount, 1, 1, pFo);
			fwrite(&Ex_Vols, sizeof(VGMX_CHIP_DATA16), Ex_VolCount, pFo);
		}
		UINT8 PADDING[16] = { 0 };
		fwrite(PADDING, 1, padsize, pFo);
	}
	fwrite(vgm_data, 1, vgm_dlen, pFo);
	return 0;
}

