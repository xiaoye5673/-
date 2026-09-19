#ifndef __FS4418_LED_H__
#define __FS4418_LED_H__

#define LED_MAGIC 'L'
#define LED_ON	_IOW(LED_MAGIC, 0, int)
#define LED_OFF	_IOW(LED_MAGIC, 1, int)


#define RED_LED   'r'
#define GREEN_LED 'g'
#define BLUE_LED  'b'

#endif
