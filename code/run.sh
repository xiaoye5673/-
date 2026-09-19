完整 `run.sh`：

```sh
#!/bin/sh
cd /app
echo "===== 启动危化品监控终端 ====="
killall monitor 2>/dev/null
sleep 1

# 卸载旧驱动
rmmod fs6818_led   2>/dev/null
rmmod fs6818_pwm   2>/dev/null
rmmod beep_driver  2>/dev/null
rmmod adc_driver   2>/dev/null
sleep 1

# 加载驱动
insmod adc_driver.ko
echo "[√] ADC驱动已加载"

insmod fs6818_pwm.ko
echo "[√] 蜂鸣器PWM驱动已加载"
sleep 0.2
./beep_off                          # 驱动加载后立即关闭蜂鸣器，避免启动时乱响

insmod fs6818_led.ko
mknod /dev/newled c 500 0 2>/dev/null   # LED驱动主设备号500，需手动创建设备节点
echo "[√] RGB LED驱动已加载"

sleep 1
echo "设备节点："
ls /dev/adc /dev/pwm /dev/newled /dev/i2c-2 2>/dev/null
echo ""
echo "启动monitor..."
echo "================================"
./monitor
```