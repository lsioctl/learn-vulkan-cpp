#include "glm/gtc/matrix_transform.hpp"

#include "camera.hpp"


Camera::Camera() :
    position_{glm::vec3(0.0f, 0.0f,  3.0f)},
    front_{glm::vec3(0.0f, 0.0f, -1.0f)},
    // y axis inversion compared to OpenGL (so glm)
    up_{glm::vec3(0.0f, -1.0f,  0.0f)},
    first_mouse_event_{false},
    pitch_{0.},
    // camera points toward negative z-axis, so 90 clockwise
    // positive degrees rotate counter clockwise
    yaw_{-90.},
    sensitivity_{0.1},
    // First mouse position (screen 800*600)
    // it is not really used (see mouse callback)
    last_mouse_x_{400.},
    last_mouse_y_{300.},
    speed_coeff_{2.5f}
{
}

void Camera::updateOrientation(double mouse_x_pos, double mouse_y_pos)
{
    if (first_mouse_event_ == true)
    {
        last_mouse_x_ = mouse_x_pos;
        last_mouse_y_ = mouse_y_pos;
        first_mouse_event_ = false;
    }
  
    float x_offset = mouse_x_pos - last_mouse_x_;
    float y_offset = last_mouse_y_ - mouse_y_pos; 
    last_mouse_x_ = mouse_x_pos;
    last_mouse_y_ = mouse_y_pos;

    x_offset *= sensitivity_;
    y_offset *= sensitivity_;

    yaw_   += x_offset;
    pitch_ += y_offset;

    // Ensure no weird camera movement are possible
    // And also because LookAt flips when pitch is parallel to the
    // world's up vector
    if(pitch_ > 89.0f)
        pitch_ = 89.0f;
    if(pitch_ < -89.0f)
        pitch_ = -89.0f;

    glm::vec3 direction;
    direction.x = cos(glm::radians(yaw_)) * cos(glm::radians(pitch_));
    direction.y = sin(glm::radians(pitch_));
    direction.z = sin(glm::radians(yaw_)) * cos(glm::radians(pitch_));
    front_ = glm::normalize(direction);
}

// Change the global variable camera_pos
// takes global variables camera_front, camera_up, delta_time
// as hidden parameters
void Camera::updatePosition(Movement camera_movement, float delta_time)
{
    // ensure the speed is the same regardless of the frame rate
    const float camera_speed = speed_coeff_ * delta_time;

    if (camera_movement == Movement::Front)
    {
        position_ += camera_speed * front_;
    }
    if (camera_movement == Movement::Back)
    {
        position_ -= camera_speed * front_;
    }
    if (camera_movement == Movement::Left)
    {
        position_ -= glm::normalize(glm::cross(front_, up_)) * camera_speed;
    }
    if (camera_movement == Movement::Right)
    {
        position_ += glm::normalize(glm::cross(front_, up_)) * camera_speed;
    }
}

const glm::mat4& Camera::getUpdatedViewMatrix() {
    view_matrix_ = glm::lookAt(
        position_, // eye
        position_ + front_, // center
        up_ // up
    );

    return view_matrix_;
}

const glm::vec3& Camera::getPosition() const {
    return position_;
}

const glm::vec3& Camera::getFront() const {
    return front_;
}

float Camera::getYaw() const {
    return yaw_;
}

float Camera::getPitch() const {
    return pitch_;
}