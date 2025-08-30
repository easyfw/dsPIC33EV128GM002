#ifndef SHT4X_H_ // Start of SHT4X_H_ 
#define SHT4X_H_

#define SHT4X_ADDR                              0x45
#define SHT4X_WRITE_ADDR                (SHT4X_ADDR << 1)
#define SHT4X_READ_ADDR                 ((SHT4X_ADDR << 1) | 1)
#define SHT4X_CMD_MEASURE_HPM   0xFD 

bool SHT4x_Read_TH (float *T, float *H);

#endif // End of SHT4X_H_ 