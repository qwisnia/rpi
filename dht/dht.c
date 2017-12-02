/*
 *  dht.c:
 *	read temperature and humidity from DHT11 or DHT22 sensor
 */

 /*
	ATTACH DATABASE 'roomMeasurments.db' as 'roomMeasurments';
	DROP TABLE roomMeasurments.dht;
	SELECT * FROM roomMeasurments.dht;
 */

#include <wiringPi.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <sqlite3.h>
#include <stdio.h>
#include <string.h>
#include <sys/timeb.h>
#include <stdbool.h>
#include <time.h>

#define DHT_PIN      3 /* GPIO-22 */
#define DB_PATH      "/home/pi/programming/c/hello_world/dht/roomMeasurments.db"

#define TEMPERATURE_BITS 16
#define HUMIDITY_BITS 16
#define CHECKUM_BITS 8

#define BUFFER_BITS (TEMPERATURE_BITS + HUMIDITY_BITS + CHECKUM_BITS)

#define BUFFER_BYTES (BUFFER_BITS / 8)

//Using delayMicroseconds lets the linux scheduler decide to jump to another process.  Using this function avoids letting the
//scheduler know we are pausing and provides much faster operation if you are needing to use lots of delays.
void delayNanosecondsNoSleep (int delay_ns)
{
	long int start_time;
	long int time_difference;
	struct timespec gettime_now;

	clock_gettime(CLOCK_REALTIME, &gettime_now);
	start_time = gettime_now.tv_nsec;		//Get nS value
	while (1)
	{
		clock_gettime(CLOCK_REALTIME, &gettime_now);
		time_difference = gettime_now.tv_nsec - start_time;

		if (time_difference < 0)
			time_difference += 1000000000;				//(Rolls over every 1 second)
		if (time_difference > delay_ns)		//Delay for # nS
			break;
	}
}

int create_db() {

	//	ATTACH DATABASE 'roomMeasurments.db' as 'roomMeasurments';
	//	DROP TABLE roomMeasurments.dht;
	//
	//	CREATE TABLE roomMeasurments.dht(
	//	   TIME           INT       PRIMARY KEY NOT NULL,
	//	   HUMIDITY       NUMERIC   NOT NULL,
	//	   TEMPERATURE    NUMERIC   NOT NULL
	//	);

    return 0;
}

int read_dht_data(float* temperature, float* humidity) {

    uint8_t *data     = NULL;
    uint8_t bit_cnt   = 0;
    struct timespec gettime_now;
    long int start_time = 0;
    long int time_difference = 0;

    data = malloc(BUFFER_BYTES);
    memset(data, 0x00, BUFFER_BYTES);
    
	/* pull pin down for 1 ms */
    pinMode(DHT_PIN, OUTPUT);
    digitalWrite(DHT_PIN, LOW);
	delayNanosecondsNoSleep(1000000);

    /* prepare to read the pin */
    pinMode(DHT_PIN, INPUT);

	while (digitalRead(DHT_PIN) == HIGH);
    while (digitalRead(DHT_PIN) == LOW);
    while (digitalRead(DHT_PIN) == HIGH);
    
    while (bit_cnt < 40) {
    	
        while (digitalRead(DHT_PIN) == LOW) {
        	clock_gettime(CLOCK_REALTIME, &gettime_now);
        	start_time = gettime_now.tv_nsec;
        }

		/*
		 * max 70us HIGH
		 * 70 us = '1'
		 * 26 - 28 us = '0'
		 */
		while (digitalRead(DHT_PIN) == HIGH) {
			clock_gettime(CLOCK_REALTIME, &gettime_now);

			time_difference = gettime_now.tv_nsec - start_time;
			if (time_difference < 0) {
				time_difference += 1000000000; // (Rolls over every 1 second)
			}

			if (time_difference > 90000) {
				fprintf(stdout, "time_difference = %ld\nbit_num = %d\n", time_difference, bit_cnt);
				return -1;
			}
		}

		data[bit_cnt / 8] <<= 1;
		if (time_difference > 30000) {
			data[bit_cnt / 8] |= 1;
		}
		
		bit_cnt++;
    }

	/* checksum is the last 8 bit of data sum */
    if (data[4] != (uint8_t)((data[0] + data[1] + data[2] + data[3]) & 0xFF)) {
    	return -2;
    }

    // calculate temperature and humidity
	*humidity = (float)((data[0] << 8) + data[1]) / 10;
	*temperature = (float)(((data[2] & 0x7F) << 8) + data[3]) / 10;

	free(data);
    return 0;
}

int main( void )
{
    sqlite3 *conn;
    char *sql = NULL;
    struct timeb tmb;
    int8_t ret = -1;

    printf("Raspberry Pi DHT11/DHT22 temperature and humidity measurement program\n");

    if (wiringPiSetup() == -1) {
        return(1);
    }

    if (sqlite3_open(DB_PATH, &conn)) {
        printf("Can't open the database");
        return(1);
    }

    float temperature;
    float humidity;
	char *err_msg = 0;

	ftime(&tmb);
	while(ret != 0) {
		ret = read_dht_data(&temperature, &humidity);
		if (ret != 0) {
			delay(1000);
		}
	}

    asprintf(&sql, "INSERT INTO dht VALUES(%ld, %.1f, %.1f);", tmb.time, humidity, temperature);
    printf("%s\n", sql);

    int rc = sqlite3_exec(conn, sql, 0, 0, &err_msg);

    if (rc != SQLITE_OK) {
        fprintf(stderr, "Failed to insert data\n");
        fprintf(stderr, "SQL error: %s\n", err_msg);
        sqlite3_free(err_msg);
    } else {
        fprintf(stdout, "Data inserted\n");
    }

    sqlite3_close(conn);

    return(0);
}
