// Pico W + GY-511 (LSM303D): acceleration, pitch/roll, compass heading
// Wiring: SDA -> GP2 (pin 4), SCL -> GP3 (pin 5), VCC -> 3V3 (pin 36), GND -> GND
// CMake: target_link_libraries(<name> pico_stdlib hardware_i2c)

#include <stdio.h>
#include <math.h>
#include "pico/stdlib.h"
#include "hardware/i2c.h"

#define I2C_PORT   i2c1
#define SDA_PIN    2  // GP2 to SDA
#define SCL_PIN    3  // GP3 to SCL

#define DEV_ADDR   0x1D
#define WHO_AM_I   0x0F
#define OUT_X_L_M  0x08
#define CTRL1      0x20
#define CTRL2      0x21
#define CTRL5      0x24
#define CTRL6      0x25
#define CTRL7      0x26
#define OUT_X_L_A  0x28

#define RAD2DEG    57.2957795f
#define ACC_G_PER_LSB 0.000061f   // +/-2 g full scale
#define CAL_TIME_MS   20000       // rotate the board during this time

typedef struct { float x, y, z; } vec3;

static int reg_write(uint8_t reg, uint8_t val) {
    uint8_t b[2] = {reg, val};
    return i2c_write_blocking(I2C_PORT, DEV_ADDR, b, 2, false);
}

static int reg_read(uint8_t reg, uint8_t *dst, size_t n) {
    if (n > 1) reg |= 0x80;  // auto-increment
    int r = i2c_write_blocking(I2C_PORT, DEV_ADDR, &reg, 1, true);
    if (r < 0) return r;
    return i2c_read_blocking(I2C_PORT, DEV_ADDR, dst, n, false);
}

static vec3 cross(vec3 a, vec3 b) {
    return (vec3){a.y * b.z - a.z * b.y,
                  a.z * b.x - a.x * b.z,
                  a.x * b.y - a.y * b.x};
}

static float dot(vec3 a, vec3 b) { return a.x * b.x + a.y * b.y + a.z * b.z; }

static vec3 normalize(vec3 v) {
    float n = sqrtf(dot(v, v));
    if (n < 1e-6f) return v;
    return (vec3){v.x / n, v.y / n, v.z / n};
}

static const char *compass_point(float deg) {
    static const char *names[] = {"N","NE","E","SE","S","SW","W","NW"};
    return names[(int)((deg + 22.5f) / 45.0f) % 8];
}

int main() {
    stdio_init_all();
    sleep_ms(3000);

    i2c_init(I2C_PORT, 400 * 1000);
    gpio_set_function(SDA_PIN, GPIO_FUNC_I2C);
    gpio_set_function(SCL_PIN, GPIO_FUNC_I2C);
    gpio_pull_up(SDA_PIN);
    gpio_pull_up(SCL_PIN);

    uint8_t who = 0;
    if (reg_read(WHO_AM_I, &who, 1) < 0 || who != 0x49) {
        printf("LSM303D not found (WHO_AM_I=0x%02X, expected 0x49). Check wiring.\n", who);
        while (true) sleep_ms(1000);
    }

    // Accelerometer: 100 Hz, block data update, XYZ enabled, +/-2 g
    reg_write(CTRL1, 0x5F);
    reg_write(CTRL2, 0x00);
    // Magnetometer: high resolution, 50 Hz, +/-2 gauss, continuous conversion
    reg_write(CTRL5, 0x70);
    reg_write(CTRL6, 0x00);
    reg_write(CTRL7, 0x00);

    // Running min/max for hard-iron calibration
    vec3 mn = { 32767,  32767,  32767};
    vec3 mx = {-32768, -32768, -32768};
    absolute_time_t start = get_absolute_time();

    printf("Rotate the board slowly in all directions for %d s to calibrate the compass...\n",
           CAL_TIME_MS / 1000);

    while (true) {
        uint8_t a[6], m[6];
        if (reg_read(OUT_X_L_A, a, 6) < 0 || reg_read(OUT_X_L_M, m, 6) < 0) {
            printf("I2C read failed\n");
            sleep_ms(200);
            continue;
        }

        // Raw -> g
        vec3 acc = {
            (int16_t)((a[1] << 8) | a[0]) * ACC_G_PER_LSB,
            (int16_t)((a[3] << 8) | a[2]) * ACC_G_PER_LSB,
            (int16_t)((a[5] << 8) | a[4]) * ACC_G_PER_LSB
        };

        // Raw magnetometer counts
        vec3 mag = {
            (float)(int16_t)((m[1] << 8) | m[0]),
            (float)(int16_t)((m[3] << 8) | m[2]),
            (float)(int16_t)((m[5] << 8) | m[4])
        };

        bool calibrating = absolute_time_diff_us(start, get_absolute_time()) < (int64_t)CAL_TIME_MS * 1000;
        if (calibrating) {
            if (mag.x < mn.x) mn.x = mag.x;  if (mag.x > mx.x) mx.x = mag.x;
            if (mag.y < mn.y) mn.y = mag.y;  if (mag.y > mx.y) mx.y = mag.y;
            if (mag.z < mn.z) mn.z = mag.z;  if (mag.z > mx.z) mx.z = mag.z;
        }

        // Apply hard-iron offset: (max + min) / 2
        mag.x -= (mx.x + mn.x) / 2.0f;
        mag.y -= (mx.y + mn.y) / 2.0f;
        mag.z -= (mx.z + mn.z) / 2.0f;

        // Tilt angles (degrees)
        float roll  = atan2f(acc.y, acc.z) * RAD2DEG;
        float pitch = atan2f(-acc.x, sqrtf(acc.y * acc.y + acc.z * acc.z)) * RAD2DEG;

        // Tilt-compensated heading of the board's +X axis
        vec3 east  = normalize(cross(mag, acc));
        vec3 north = normalize(cross(acc, east));
        float heading = atan2f(east.x, north.x) * RAD2DEG;
        if (heading < 0) heading += 360.0f;

        if (calibrating) {
            printf("CALIBRATING  Accel[g] X:%6.3f Y:%6.3f Z:%6.3f\n", acc.x, acc.y, acc.z);
        } else {
            printf("Accel[g] X:%6.3f Y:%6.3f Z:%6.3f | Pitch:%6.1f Roll:%6.1f | Heading:%6.1f %s\n",
                   acc.x, acc.y, acc.z, pitch, roll, heading, compass_point(heading));
        }
        sleep_ms(200);
    }
}