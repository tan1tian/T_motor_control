#include <pigpio.h>
#include <iostream>
#include <thread>
#include <atomic>
#include <chrono>
#include <csignal>
#include <cstdlib>

// TB6612FNG 引脚设置
const int AIN1 = 17;  // GPIO 17
const int AIN2 = 27;  // GPIO 27
const int PWMA = 22;  // GPIO 22
const int STBY = 5;   // GPIO 5

// 霍尔编码器引脚
const int ENCODER_PIN_A = 23; // GPIO 23
const int ENCODER_PIN_B = 24; // GPIO 24

// 减速比和编码器脉冲数
const int REDUCTION_RATIO = 30;
const int PULSES_PER_ROTATION = 22;

// 一圈的角度（360度）
const float FULL_ROTATION_ANGLE = 360.0f;

// 全局变量
std::atomic<bool> stop(false);
std::atomic<int> pulse_count_A(0);
std::atomic<int> pulse_count_B(0);

// 设置电机速度
void set_motor_speed(int speed) {
    if (speed > 0) {
        gpioWrite(AIN1, 1);
        gpioWrite(AIN2, 0);
    } else if (speed < 0) {
        gpioWrite(AIN1, 0);
        gpioWrite(AIN2, 1);
    } else {
        gpioWrite(AIN1, 0);
        gpioWrite(AIN2, 0);
    }
    gpioPWM(PWMA, abs(speed));
}

// 读取霍尔编码器脉冲
void read_encoder_pulse(int gpio, int level, uint32_t tick, void* userdata) {
    if (gpio == ENCODER_PIN_A) {
        pulse_count_A++;
    } else if (gpio == ENCODER_PIN_B) {
        pulse_count_B++;
    }
}

// 清理GPIO并退出程序
void cleanup(int signum) {
    gpioPWM(PWMA, 0);
    gpioTerminate();
    exit(signum);
}

// 采样角速度
void sample_angular_velocity() {
    while (!stop) {
        int total_pulses = (pulse_count_A.load() + pulse_count_B.load()) / 2; // 取平均值
        float angular_velocity = (FULL_ROTATION_ANGLE / PULSES_PER_ROTATION) * (total_pulses / REDUCTION_RATIO);
        std::cout << "Motor angular velocity: " << angular_velocity << " degrees per second" << std::endl;
        pulse_count_A.store(0);  // 重置计数
        pulse_count_B.store(0);  // 重置计数
        std::this_thread::sleep_for(std::chrono::milliseconds(1000)); // 每隔1秒采样一次
    }
}

int main() {
    // 初始化pigpio
    if (gpioInitialise() < 0) {
        std::cerr << "pigpio initialization failed" << std::endl;
        return 1;
    }

    // 设置GPIO模式
    gpioSetMode(AIN1, PI_OUTPUT);
    gpioSetMode(AIN2, PI_OUTPUT);
    gpioSetMode(STBY, PI_OUTPUT);
    gpioSetMode(PWMA, PI_OUTPUT);
    gpioSetMode(ENCODER_PIN_A, PI_INPUT);
    gpioSetMode(ENCODER_PIN_B, PI_INPUT);

    // 设置STBY为高电平，使电机工作
    gpioWrite(STBY, 1);

    // 设置PWM频率
    gpioSetPWMfrequency(PWMA, 1000);  // 1kHz

    // 设置霍尔编码器回调函数
    gpioSetAlertFuncEx(ENCODER_PIN_A, read_encoder_pulse, nullptr);
    gpioSetAlertFuncEx(ENCODER_PIN_B, read_encoder_pulse, nullptr);

    // 设置Ctrl+C信号处理函数
    std::signal(SIGINT, cleanup);

    // 创建一个线程来采样角速度
    std::thread velocity_thread(sample_angular_velocity);

    // 设置电机速度为50%
    set_motor_speed(128);

    // 主线程延迟
    std::this_thread::sleep_for(std::chrono::seconds(10));

    // 设置停止标志并等待线程结束
    stop = true;
    velocity_thread.join();

    // 清理GPIO并退出程序
    gpioTerminate();
    return 0;
}
