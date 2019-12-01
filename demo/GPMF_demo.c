/*! @file GPMF_demo.c
 *
 *  @brief Demo to extract GPMF from an MP4
 *
 *  @version 1.5.0
 *
 *  (C) Copyright 2017 GoPro Inc (http://gopro.com/).
 *	
 *  Licensed under either:
 *  - Apache License, Version 2.0, http://www.apache.org/licenses/LICENSE-2.0  
 *  - MIT license, http://opensource.org/licenses/MIT
 *  at your option.
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 *
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>

#include "../GPMF_parser.h"
#include "GPMF_mp4reader.h"
#include "anglemath.hpp"

#define DEBUG 0

extern void PrintGPMF(GPMF_stream *ms);

int main(int argc, char *argv[])
{
	int32_t ret = GPMF_OK;
	GPMF_stream metadata_stream, *ms = &metadata_stream;
	double metadatalength;
	double start_offset = 0.0;
	uint32_t *payload = NULL; //buffer to store GPMF samples from the MP4.


	// get file return data
	if (argc != 2)
	{
		printf("usage: %s <file_with_GPMF>\n", argv[0]);
		return -1;
	}

	size_t mp4 = OpenMP4Source(argv[1], MOV_GPMF_TRAK_TYPE, MOV_GPMF_TRAK_SUBTYPE);

	if (mp4 == 0)
	{
		printf("error: %s is an invalid MP4/MOV or it has no GPMF data\n", argv[1]);
		return -1;
	}

	metadatalength = GetDuration(mp4);

	if (metadatalength > 0.0)
	{
		uint32_t index, payloads = GetNumberPayloads(mp4);
		if (DEBUG) printf("found %.2fs of metadata, from %d payloads, within %s\n", metadatalength, payloads, argv[1]);

		uint32_t fr_num, fr_dem;
		uint32_t frames = GetVideoFrameRateAndCount(mp4, &fr_num, &fr_dem);
		double gyro_offset, gyro_end;
		uint32_t current_image_frame=0, current_gyro_frame=0;
		frame_modifier_t frame_adjustments[4];
		angles_t current_angles, previous_angles;
		
		//GoPro Hero 6, pixel per radian: 1820
		/*frame_adjustments[0].original.set_cartesian(960, 540);
		frame_adjustments[1].original.set_cartesian(960, 1620);
		frame_adjustments[2].original.set_cartesian(2880, 540);
		frame_adjustments[3].original.set_cartesian(2880, 1620);*/
		
		frame_adjustments[0].original.set_cartesian(480*2, 270*2);
		frame_adjustments[1].original.set_cartesian(-480*2, 270*2);
		frame_adjustments[2].original.set_cartesian(480*2, -270*2);
		frame_adjustments[3].original.set_cartesian(-480*2, -270*2); //960+540
		
		frame_adjustments[0].reset();
		frame_adjustments[1].reset();
		frame_adjustments[2].reset();
		frame_adjustments[3].reset();

		double gyro_rate = GetGPMFSampleRate(mp4, STR2FOURCC("GYRO"), GPMF_SAMPLE_RATE_PRECISE, &gyro_offset, &gyro_end);// GPMF_SAMPLE_RATE_FAST);
        gyro_offset+=(0.02*3);
		if (frames)
		{
			if (DEBUG) printf("video framerate is %.2f with %d frames\n", (float)fr_num/(float)fr_dem, frames);
			if (DEBUG) printf("GYRO samplerate is %.2f with duration of %lf s\n", (float)gyro_rate, gyro_end-gyro_offset);
			if (DEBUG) printf("GYRO OFFSET: %lf s\n", gyro_offset);
		}
		//current_image_frame = gyro_offset * ((float)fr_num/(float)fr_dem);

		printf("VID.STAB 1\r\n");
		//printf("Frame 1 (List 0 [])\r\n");


		for (index = 0; index < payloads; index++)
		{
			uint32_t payloadsize = GetPayloadSize(mp4, index);
			double in = 0.0, out = 0.0; //times
			payload = GetPayload(mp4, payload, index);
			if (payload == NULL)
				goto cleanup;

			ret = GetPayloadTime(mp4, index, &in, &out);
			if (ret != GPMF_OK)
				goto cleanup;

			ret = GPMF_Init(ms, payload, payloadsize);
			if (ret != GPMF_OK)
				goto cleanup;


				if (GPMF_OK == GPMF_FindNext(ms, STR2FOURCC("GYRO"), GPMF_RECURSE_LEVELS)) //GoPro Hero5/6/7 Accelerometer
				{
					uint32_t key = GPMF_Key(ms);
					uint32_t samples = GPMF_Repeat(ms);
					uint32_t elements = GPMF_ElementsInStruct(ms);
					uint32_t buffersize = samples * elements * sizeof(double);
					GPMF_stream find_stream;
					double *ptr, *tmpbuffer = (double *)malloc(buffersize);
					char units[10][6] = { "" };
					uint32_t unit_samples = 1;

					if (DEBUG) printf("MP4 Payload time %.3f to %.3f seconds\n", in, out);

					if (tmpbuffer && samples)
					{
						uint32_t i, j;

						//Search for any units to display
						GPMF_CopyState(ms, &find_stream);
						if (GPMF_OK == GPMF_FindPrev(&find_stream, GPMF_KEY_SI_UNITS, GPMF_CURRENT_LEVEL) ||
							GPMF_OK == GPMF_FindPrev(&find_stream, GPMF_KEY_UNITS, GPMF_CURRENT_LEVEL))
						{
							char *data = (char *)GPMF_RawData(&find_stream);
							int ssize = GPMF_StructSize(&find_stream);
							unit_samples = GPMF_Repeat(&find_stream);

							for (i = 0; i < unit_samples; i++)
							{
								memcpy(units[i], data, ssize);
								units[i][ssize] = 0;
								data += ssize;
							}
						}

						//GPMF_FormattedData(ms, tmpbuffer, buffersize, 0, samples); // Output data in LittleEnd, but no scale
						GPMF_ScaledData(ms, tmpbuffer, buffersize, 0, samples, GPMF_TYPE_DOUBLE);  //Output scaled data as floats

						ptr = tmpbuffer;
						for (i = 0; i < samples; i++)
						{
							if (DEBUG) printf("Image frame: %d - ", (int)((gyro_offset+((float)current_gyro_frame/gyro_rate)) * ((float)fr_num/(float)fr_dem)) );
							while (current_image_frame < (int)((gyro_offset+((float)current_gyro_frame/gyro_rate)) * ((float)fr_num/(float)fr_dem)) ){
								printf("Frame %d (List 4 [", ++current_image_frame);
								for (int k=0; k<4; k++){
									//LocalMotions ::= List [(LM v.x v.y f.x f.y f.size contrast match),...]
									printf("(LM %d %d %d %d 128 0.8 0.01)",
										(int)round(frame_adjustments[k].original.x - frame_adjustments[k].adjust.x),
										(int)round(frame_adjustments[k].original.y - frame_adjustments[k].adjust.y),
										(int)round(frame_adjustments[k].original.x + 960*2),
										(int)round(frame_adjustments[k].original.y + 540*2) );
										if (k<3) printf(",");

									frame_adjustments[k].reset();
								}
								printf("])\r\n");
							}
							if (DEBUG>1) printf("GYRODATA: %d - ", current_gyro_frame);
							if (DEBUG>1) printf("%c%c%c%c ", PRINTF_4CC(key));
							
								if (DEBUG>1) printf("%.10f%s, %.10f%s, %.10f%s\n", *ptr, units[0], *(ptr+1), units[0], *(ptr+2), units[0]);
								current_angles.x = (*(ptr)); //plus pan
								current_angles.y = -(*(ptr+1)); //minus tilt
								current_angles.z = -(*(ptr+2)); // ?! roll
								
								for (int k=0; k<4; k++){
									
									frame_adjustments[k].adjust.set_cartesian(
										frame_adjustments[k].adjust.x + current_angles.x*((1920*2*(180/118.2))/3.14)*(1/gyro_rate), 
										frame_adjustments[k].adjust.y + current_angles.y*((1920*2*(180/118.2))/3.14)*(1/gyro_rate));
										
									
									frame_adjustments[k].adjust.set_polar(
										frame_adjustments[k].adjust.r,
										frame_adjustments[k].adjust.t + current_angles.z*(1/gyro_rate));
										
								}

								ptr+=3;
							//check if we should start a new frame

							current_gyro_frame++;
							if (DEBUG>1) printf("\n");

						}
						free(tmpbuffer);
					}
				}
				GPMF_ResetState(ms);
				if (DEBUG) printf("\n");
		}

#if 0
		// Find all the available Streams and compute they sample rates
		while (GPMF_OK == GPMF_FindNext(ms, GPMF_KEY_STREAM, GPMF_RECURSE_LEVELS))
		{
			if (GPMF_OK == GPMF_SeekToSamples(ms)) //find the last FOURCC within the stream
			{
				double start, end;
				uint32_t fourcc = GPMF_Key(ms);
				double rate = GetGPMFSampleRate(mp4, fourcc, GPMF_SAMPLE_RATE_PRECISE, &start, &end);// GPMF_SAMPLE_RATE_FAST);

				printf("%c%c%c%c sampling rate = %fHz (time %f to %f)\",\n", PRINTF_4CC(fourcc), rate, start, end);
			}
		}
#endif


	cleanup:
		if (payload) FreePayload(payload); payload = NULL;
		CloseSource(mp4);
	}

	return ret;
}
