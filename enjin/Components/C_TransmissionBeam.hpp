#ifndef C_TRANSMISSIONBEAM_HPP
#define C_TRANSMISSIONBEAM_HPP

#include <vector>
#include <memory>

#include "Component.hpp"
#include "C_Drawable.hpp"
#include "C_Position.hpp"
#include <Adafruit_GFX.h>
#include "enjin2_compat.hpp"
#include "../utils/Polar.hpp"

#include "../Object.hpp"

namespace enjin
{
    class C_TransmissionBeam : public C_Drawable
    {
    public:
        enum
        {
            IDLE = 0,
            PHASE_EDIT,
            WIDTH_EDIT
        };

        C_TransmissionBeam(Object *owner, uint8_t width, uint8_t height) : C_Drawable(width, height), Component(owner),
                                                                           aim_point{(64, 127), (64, 127), (64, 127)},
                                                                           width(0.0f),
                                                                           phase(0.5f),
                                                                           color(12),
                                                                           phase_angle(0),
                                                                           half_width(0),
                                                                           start_angle(0),
                                                                           end_angle(0),
                                                                           mode(IDLE)
        {
            position = owner->GetComponent<C_Position>();

            if (!position)
            {
            }
            full_color = color;
            dimmed_color = color / 2;
        };
        void Awake() override {};
        void Update(uint16_t deltaTime) override
        {
            if (mode == WIDTH_EDIT)
            {
                timer += deltaTime;
                if (timer >= 240)
                { // Change the flashing speed by adjusting this value
                    timer = 0;
                    is_dimmed = !is_dimmed; // Toggle the brightness state
                    if (!is_dimmed)
                    {
                        color = dimmed_color;
                    }
                    else
                    {
                        color = full_color;
                    }
                }
            }
            else if (mode == PHASE_EDIT)
            {
            }
            else
            {
                color = full_color;
            }
        }
        void Draw(EiseiCanvas &canvas) override
        {
            fillArcWithTriangles(canvas, 64, 64, 70, start_angle, end_angle, (color - 6) / 4);
            canvas.drawLine(64, 64, aim_point[0].x, aim_point[0].y, color);
            canvas.drawLine(64, 64, aim_point[1].x, aim_point[1].y, color);
            canvas.drawLine(64, 64, aim_point[2].x, aim_point[2].y, color);
        };
        bool ContinueToDraw() const override
        {
            return !owner->IsQueuedForRemoval();
        };

        void SetMode(int mode)
        {
            this->mode = mode;
        }

        void SetPhase(float phase)
        {
            this->phase = phase;
            start_angle = (uint16_t)((phase - width * 0.5f) * 360.0f);
            end_angle = (uint16_t)((phase + width * 0.5f) * 360.0f);
            aim_point[0] = RadialToCartesian(phase, 63, Vector2(63, 63));
            aim_point[1] = RadialToCartesian(phase - width * 0.5f, 63, Vector2(63, 63));
            aim_point[2] = RadialToCartesian(phase + width * 0.5f, 63, Vector2(63, 63));
        };
        void SetWidth(float width)
        {
            this->width = width;
            start_angle = (uint16_t)((phase - width * 0.5f) * 360.0f);
            end_angle = (uint16_t)((phase + width * 0.5f) * 360.0f);
            aim_point[0] = RadialToCartesian(phase, 63, Vector2(63, 63));
            aim_point[1] = RadialToCartesian(phase - width * 0.5f, 63, Vector2(63, 63));
            aim_point[2] = RadialToCartesian(phase + width * 0.5f, 63, Vector2(63, 63));
        };

        void DrawBackground(EiseiCanvas &canvas) {};

    private:
        float width, phase;
        uint8_t color, full_color, dimmed_color;
        int16_t phase_angle, half_width;
        Vector2 aim_point[3];
        int16_t start_angle, end_angle;

        int mode;
        uint8_t timer = 0;
        bool is_dimmed = false;

        void drawDottedArc(EiseiCanvas &canvas, int x0, int y0, int radius, int startAngle, int endAngle, uint16_t color)
        {
            // Convert start and end angles to radians
            float startRad = startAngle * PI / 180.0;
            float endRad = endAngle * PI / 180.0;
            float midRad = (startRad + endRad) / 2.0;

            // Calculate the number of segments to use for the arc
            int numSegments = abs(endAngle - startAngle) / 7; // Adjust the denominator to change the smoothness of the arc

            // Calculate the angle between each segment
            float angleStep = (endRad - startRad) / numSegments;

            // Draw each segment of the arc
            for (int i = 0; i <= numSegments; i++)
            {
                float angle = startRad + i * angleStep;
                int x = x0 + radius * -cos(angle);
                int y = y0 + radius * -sin(angle);

                // Calculate the distance of the current angle from the middle of the arc
                float distance = abs(angle - midRad);

                // Use the distance to calculate the color
                uint16_t currentColor = color * (1.0 - (distance / (endRad - startRad)));

                canvas.drawPixel(x, y, currentColor);
            }
        }

        void fillArcWithTriangles(EiseiCanvas &canvas, int x0, int y0, int radius, int startAngle, int endAngle, uint16_t color)
        {
            // Convert start and end angles to radians
            float startRad = startAngle * PI / 180.0;
            float endRad = endAngle * PI / 180.0;

            // Fixed angle span for each triangle (in degrees)
            float triangleAngleSpan = 45.0;

            // Calculate the number of triangles needed
            int numTriangles = std::ceil((endAngle - startAngle) / triangleAngleSpan);

            // Calculate the angle step for each triangle
            float angleStep = (endRad - startRad) / numTriangles;

            // Draw each triangle
            for (int i = 0; i < numTriangles; i++)
            {
                float angle1 = startRad + i * angleStep;
                float angle2 = angle1 + angleStep;

                // Calculate the vertices on the perimeter of the arc
                int x1 = x0 - radius * cos(angle1);
                int y1 = y0 - radius * sin(angle1); // Adjust for screen coordinates
                int x2 = x0 - radius * cos(angle2);
                int y2 = y0 - radius * sin(angle2);

                // Draw the triangle
                canvas.fillTriangle(x0, y0, x1, y1, x2, y2, color);
            }
        }
    };
}
#endif // C_TRANSMISSIONBEAM_HPP
