
/*
 *Author: WismyYao
 *Date:2020-01-15
 *Description:Draw the curved
 *Based on libopenui https://github.com/opentx/libopenui.git
 */
#include <math.h>

int getAngle(int centerX, int centerY, int pointX, int pointY) {
		int x = pointX - centerX;
		int y = pointY - centerY;

		if (x == 0)
			return (y > 0) ? 180 : 0;

		int a = (int)(atan2(y, x) * 180 / M_PI);
		a += 90;

		if ((x < 0) && (y < 0))
			a += 360;

		return a;
	}

void drawPie(int x, int y, int internalRadius, int externalRadius, int startAngle, int endAngle, LcdFlags flags)
{
  int slopes[4];

  if(startAngle < -360 || endAngle > 360)
    return;

  if (startAngle < 0)
    startAngle += 360;
  if (endAngle - startAngle < 0)
    endAngle += 360;
  if (startAngle > 360)
    startAngle -= 360;
  if (endAngle - startAngle > 360)
    endAngle -= 360;

  pixel_t color = lcdColorTable[COLOR_IDX(flags)];
  APPLY_OFFSET();

  int internalDist = internalRadius * internalRadius;
  int externalDist = externalRadius * externalRadius;

  int angle;
  for (int y1 = 0; y1 <= externalRadius; y1++) {
    for (int x1 = 0; x1 <= externalRadius; x1++) {
      auto dist = x1 * x1 + y1 * y1;
      if (dist >= internalDist && dist <= externalDist) {

        angle = getAngle(x, y, x + x1, y - y1);
        if (0 <= angle && angle <= 90) {
          if (endAngle > 360) {
            if ((angle >= 0) && (angle <= endAngle -360)) {
              drawPixel(x + x1, y - y1, color);
            }
          }
          else {
            if ((angle >= startAngle) && (angle <= endAngle)) {
              drawPixel(x + x1, y - y1, color);
            }
          }
        }

        angle = getAngle(x, y, x + x1, y + y1);
        if (90 < angle && angle <= 180) {
          if (endAngle > 360) {
            if ((angle >= 0) && (angle <= endAngle - 360)) {
              drawPixel(x + x1, y + y1, color);
            }
          }
          else {
            if ((angle >= startAngle) && (angle <= endAngle)) {
              drawPixel(x + x1, y + y1, color);
            }
          }
        }

        angle = getAngle(x, y, x - x1, y + y1);
        if (180 < angle && angle <= 270) {
          if ((angle >= startAngle) && (angle <= endAngle)) {
            drawPixel(x - x1, y + y1, color);
          }
        }

        angle = getAngle(x, y, x - x1, y - y1);
        if (270 < angle && angle <= 360) {
          if ((angle >= startAngle) && (angle <= endAngle)) {
            drawPixel(x - x1, y - y1, color);
          }
        }
      }
    }
  }
}