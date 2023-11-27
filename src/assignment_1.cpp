#include <cstdlib>
#include <iostream>
#include <cmath>

#include "mygl/shader.h"
#include "mygl/mesh.h"
#include "mygl/geometry.h"
#include "mygl/camera.h"
#include "water.h"


/* translation and color for the water plane */
namespace waterPlane {
    const Vector4D color = {0.0f, 0.0f, 0.35f, 1.0f};
    const Matrix4D trans = Matrix4D::identity();
}

/* translation and scale for the scaled boat */
namespace scaledBoat {
    const Matrix4D scale = Matrix4D::scale(0.5f, 0.5f, 0.5f);
    const Matrix4D trans = Matrix4D::translation({0.0f, 0.0f, 0.0f});
}

enum struct CameraMode {
    FixedView,
    ThirdPersonView
};

/* struct holding all necessary state variables for scene */
struct {
    /* camera */
    Camera camera;
    float zoomSpeedMultiplier;
    CameraMode cameraMode;
    Vector3D cameraPositionOffsetToBoat;


    /* water */
    WaterSim waterSim;
    Water water;
    Matrix4D waterModelMatrix;

    /* boat fixed properties */
    Mesh boatMesh;
    Matrix4D boatScalingMatrix;
    Matrix4D boatTranslationMatrix;
    Matrix4D boatTransformationMatrix;
    float boatSpinRadPerSecond;
    float boatMovementPerSecond;

    /* boat dynamic properties*/
    Vector3D boatPosition;
    float boatXZAngle;

    /* shader */
    ShaderProgram shaderColor;
} sScene;

/* struct holding all state variables for input */
struct {
    bool mouseLeftButtonPressed = false;
    Vector2D mousePressStart;
    bool buttonPressed[6] = {false, false, false, false, false, false};
} sInput;

/* GLFW callback function for keyboard events */
void keyCallback(GLFWwindow *window, int key, int scancode, int action, int mods) {
    /* called on keyboard event */

    /* close window on escape */
    if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS) {
        glfwSetWindowShouldClose(window, true);
    }

    /* make screenshot and save in work directory */
    if (key == GLFW_KEY_P && action == GLFW_PRESS) {
        screenshotToPNG("screenshot.png");
    }

    /* input for boat control */
    if (key == GLFW_KEY_W) {
        sInput.buttonPressed[0] = (action == GLFW_PRESS || action == GLFW_REPEAT);
    }
    if (key == GLFW_KEY_S) {
        sInput.buttonPressed[1] = (action == GLFW_PRESS || action == GLFW_REPEAT);
    }

    if (key == GLFW_KEY_A) {
        sInput.buttonPressed[2] = (action == GLFW_PRESS || action == GLFW_REPEAT);
    }
    if (key == GLFW_KEY_D) {
        sInput.buttonPressed[3] = (action == GLFW_PRESS || action == GLFW_REPEAT);
    }

    //Switch Camera View
    if (key == GLFW_KEY_1) {
        sInput.buttonPressed[4] = (action == GLFW_PRESS || action == GLFW_REPEAT);
    }
    if (key == GLFW_KEY_2) {
        sInput.buttonPressed[5] = (action == GLFW_PRESS || action == GLFW_REPEAT);
    }

}

/* GLFW callback function for mouse position events */
void mousePosCallback(GLFWwindow *window, double x, double y) {
    /* called on cursor position change */
    if (sInput.mouseLeftButtonPressed) {
        Vector2D diff = sInput.mousePressStart - Vector2D(x, y);
        cameraUpdateOrbit(sScene.camera, diff, 0.0f);
        sInput.mousePressStart = Vector2D(x, y);
    }
}

/* GLFW callback function for mouse button events */
void mouseButtonCallback(GLFWwindow *window, int button, int action, int mods) {
    if (button == GLFW_MOUSE_BUTTON_LEFT) {
        sInput.mouseLeftButtonPressed = (action == GLFW_PRESS || action == GLFW_REPEAT);

        double x, y;
        glfwGetCursorPos(window, &x, &y);
        sInput.mousePressStart = Vector2D(x, y);
    }
}

/* GLFW callback function for mouse scroll events */
void mouseScrollCallback(GLFWwindow *window, double xoffset, double yoffset) {
    cameraUpdateOrbit(sScene.camera, {0, 0}, sScene.zoomSpeedMultiplier * yoffset);
}

/* GLFW callback function for window resize event */
void windowResizeCallback(GLFWwindow *window, int width, int height) {
    glViewport(0, 0, width, height);
    sScene.camera.width = width;
    sScene.camera.height = height;
}

/* function to set up and initialize the whole scene */
void sceneInit(float width, float height) {

    /* setup objects in scene and create opengl buffers for meshes */
    sScene.boatMesh = meshCreate(boat::vertices, boat::indices, GL_STATIC_DRAW, GL_STATIC_DRAW);
    sScene.water = waterCreate(waterPlane::color);

    /* setup transformation matrices for objects */
    sScene.waterModelMatrix = waterPlane::trans;

    sScene.boatScalingMatrix = scaledBoat::scale;
    sScene.boatTranslationMatrix = scaledBoat::trans;

    sScene.boatTransformationMatrix = Matrix4D::identity();

    sScene.boatSpinRadPerSecond = M_PI / 3.0f;
    sScene.boatMovementPerSecond = 3.0f;

    sScene.boatPosition = {0, 0, 0};
    sScene.boatXZAngle = M_PI;

    /* initialize camera */
    sScene.camera = cameraCreate(width, height, to_radians(45.0f), 0.01f, 500.0f, {10.0f, 14.0f, 10.0f},
                                 {0.0f, 4.0f, 0.0f});
    sScene.zoomSpeedMultiplier = 0.05f;
    sScene.cameraMode = CameraMode::FixedView;

    sScene.cameraPositionOffsetToBoat = sScene.camera.position - sScene.boatPosition;

    /* load shader from file */
    sScene.shaderColor = shaderLoad("shader/default.vert", "shader/default.frag");
}

/* function to move and update objects in scene (e.g., rotate boat according to user input) */
void sceneUpdate(float dt) {
//    /* update water model matrix using 3 wave functions defined in water.h */
//    for (unsigned i = 0; i < sScene.water.vertices.size(); i++) {
//        sScene.water.vertices[i].pos[1] = 0.0f;
//        for (unsigned j = 0; j < 3; j++) {
//            sScene.water.vertices[i].pos[1] += sScene.waterSim.parameter[j].amplitude *
//                                               sin(sScene.waterSim.parameter[j].omega *
//                                                   dot(sScene.waterSim.parameter[j].direction,
//                                                       Vector2D{sScene.water.vertices[i].pos[0],
//                                                                sScene.water.vertices[i].pos[2]}) +
//                                                   sScene.waterSim.accumTime * sScene.waterSim.parameter[j].phi);
//        }
//    }
//    sScene.water.mesh = meshCreate(sScene.water.vertices, grid::indices, GL_DYNAMIC_DRAW, GL_STATIC_DRAW);
//
//
//
//    /* if 'a' or 'd' are pressed: update boat rotation angle  */
//    /* only allow rotation during movement*/
//    if (sInput.buttonPressed[0] || sInput.buttonPressed[1]) {
//        if (sInput.buttonPressed[2]) {  // 'a' pressed
//            sScene.boatXZAngle += sScene.boatSpinRadPerSecond * dt;
//        } else if (sInput.buttonPressed[3]) {  // 'd' pressed
//            sScene.boatXZAngle -= sScene.boatSpinRadPerSecond * dt;
//        }
//    }
//
//
//    /* if 'w' or 's' pressed: update boat+camera position, depending on the boat rotation angle and camera mode*/
//    if (sInput.buttonPressed[0]) {  // 'w' pressed
//        sScene.boatPosition.x += std::sin(sScene.boatXZAngle + (float) M_PI_2) * sScene.boatMovementPerSecond * dt;
//        sScene.boatPosition.z += std::cos(sScene.boatXZAngle + (float) M_PI_2) * sScene.boatMovementPerSecond * dt;
//        if (sScene.cameraMode == CameraMode::ThirdPersonView) {
//            sScene.camera.position.x += std::sin(sScene.boatXZAngle + (float) M_PI_2) * sScene.boatMovementPerSecond * dt;
//            sScene.camera.position.z += std::sin(sScene.boatXZAngle + (float) M_PI_2) * sScene.boatMovementPerSecond * dt;
//        }
//    } else if (sInput.buttonPressed[1]) {  // 's' pressed
//        sScene.boatPosition.x -= std::sin(sScene.boatXZAngle + (float) M_PI_2) * sScene.boatMovementPerSecond * dt;
//        sScene.boatPosition.z -= std::cos(sScene.boatXZAngle + (float) M_PI_2) * sScene.boatMovementPerSecond * dt;
//        if (sScene.cameraMode == CameraMode::ThirdPersonView) {
//            sScene.camera.position.x -= std::sin(sScene.boatXZAngle + (float) M_PI_2) * sScene.boatMovementPerSecond * dt;
//            sScene.camera.position.z -= std::sin(sScene.boatXZAngle + (float) M_PI_2) * sScene.boatMovementPerSecond * dt;
//        }
//    }
//
//    /* apply boat rotation */
//    sScene.boatTransformationMatrix = Matrix4D::rotationY(sScene.boatXZAngle);
//
//    /* apply boat position */
//    sScene.boatTransformationMatrix =
//            Matrix4D::translation(sScene.boatPosition) * sScene.boatTransformationMatrix;
//
//
//    /* if in ThirdPersonView: update camera lookAt*/
//    if (sScene.cameraMode == CameraMode::ThirdPersonView) {
//        sScene.camera.lookAt = sScene.boatPosition;
//    }
//
//    /* if '1' or '2' is  pressed, change the camera mode*/
//    if (sInput.buttonPressed[4]) {
//        sScene.cameraMode = CameraMode::FixedView;
//    } else if (sInput.buttonPressed[5]) {
//        sScene.cameraMode = CameraMode::ThirdPersonView;
//    }


    /* update water model matrix using 3 wave functions defined in water.h */
    for (unsigned i = 0; i < sScene.water.vertices.size(); i++) {
        sScene.water.vertices[i].pos[1] = 0.0f;
        for (unsigned j = 0; j < 3; j++) {
            sScene.water.vertices[i].pos[1] += sScene.waterSim.parameter[j].amplitude *
                                               sin(sScene.waterSim.parameter[j].omega *
                                                   dot(sScene.waterSim.parameter[j].direction,
                                                       Vector2D{sScene.water.vertices[i].pos[0],
                                                                sScene.water.vertices[i].pos[2]}) +
                                                   sScene.waterSim.accumTime * sScene.waterSim.parameter[j].phi);
        }
    }
    sScene.water.mesh = meshCreate(sScene.water.vertices, grid::indices, GL_DYNAMIC_DRAW, GL_STATIC_DRAW);

    /* calculate height of water at boat's position */
    float waterHeight = 0.0f;
    for (unsigned j = 0; j < 3; j++) {
        waterHeight += sScene.waterSim.parameter[j].amplitude *
                       sin(sScene.waterSim.parameter[j].omega *
                           dot(sScene.waterSim.parameter[j].direction,
                               Vector2D{sScene.boatPosition.x, sScene.boatPosition.z}) +
                           sScene.waterSim.accumTime * sScene.waterSim.parameter[j].phi);
    }

    /* update boat translation along the y-axis */
    sScene.boatPosition.y = waterHeight;

    /* update boat rotation to align with waves */
    Vector3D triangleCenter = sScene.boatPosition;
    Vector3D triangleCorner1 = triangleCenter + Vector3D{1.0f, 0.0f, 0.0f};  // arbitrary point in the x-axis
    Vector3D triangleCorner2 = triangleCenter + Vector3D{0.0f, 0.0f, 1.0f};  // arbitrary point in the z-axis

    /* calculate triangle corner heights */
    float height1 = 0.0f;
    float height2 = 0.0f;
    float heightCenter = 0.0f;

    for (unsigned j = 0; j < 3; j++) {
        height1 += sScene.waterSim.parameter[j].amplitude *
                   sin(sScene.waterSim.parameter[j].omega *
                       dot(sScene.waterSim.parameter[j].direction, Vector2D{triangleCorner1.x, triangleCorner1.z}) +
                       sScene.waterSim.accumTime * sScene.waterSim.parameter[j].phi);

        height2 += sScene.waterSim.parameter[j].amplitude *
                   sin(sScene.waterSim.parameter[j].omega *
                       dot(sScene.waterSim.parameter[j].direction, Vector2D{triangleCorner2.x, triangleCorner2.z}) +
                       sScene.waterSim.accumTime * sScene.waterSim.parameter[j].phi);

        heightCenter += sScene.waterSim.parameter[j].amplitude *
                        sin(sScene.waterSim.parameter[j].omega *
                            dot(sScene.waterSim.parameter[j].direction, Vector2D{triangleCenter.x, triangleCenter.z}) +
                            sScene.waterSim.accumTime * sScene.waterSim.parameter[j].phi);
    }

    /* calculate vectors for the rotation matrix */
    Vector3D upVector = {0.0f, 1.0f, 0.0f};  // up vector in world space
    Vector3D boatForwardVector = normalize(triangleCorner1 - triangleCenter);
    Vector3D boatRightVector = cross(upVector, boatForwardVector);
    Vector3D boatUpVector = cross(boatForwardVector, boatRightVector);

    /* create rotation matrix to align the boat with the waves */
    Matrix4D rotationMatrix = Matrix4D::identity();
    for (int i = 0; i < 3; i++) {
        rotationMatrix[0][i] = boatRightVector[i];
        rotationMatrix[1][i] = boatUpVector[i];
        rotationMatrix[2][i] = boatForwardVector[i];
    }

    /* apply boat rotation */
    sScene.boatTransformationMatrix = rotationMatrix * sScene.boatTransformationMatrix;

    /* if 'a' or 'd' are pressed: update boat rotation angle */
    /* only allow rotation during movement*/
    if (sInput.buttonPressed[0] || sInput.buttonPressed[1]) {
        if (sInput.buttonPressed[2]) {  // 'a' pressed
            sScene.boatXZAngle += sScene.boatSpinRadPerSecond * dt;
        } else if (sInput.buttonPressed[3]) {  // 'd' pressed
            sScene.boatXZAngle -= sScene.boatSpinRadPerSecond * dt;
        }
    }

    /* if 'w' or 's' pressed: update boat+camera position, depending on the boat rotation angle and camera mode*/
    if (sInput.buttonPressed[0]) {  // 'w' pressed
        sScene.boatPosition.x += std::sin(sScene.boatXZAngle + (float) M_PI_2) * sScene.boatMovementPerSecond * dt;
        sScene.boatPosition.z += std::cos(sScene.boatXZAngle + (float) M_PI_2) * sScene.boatMovementPerSecond * dt;
        if (sScene.cameraMode == CameraMode::ThirdPersonView) {
            sScene.camera.position.x +=
                    std::sin(sScene.boatXZAngle + (float) M_PI_2) * sScene.boatMovementPerSecond * dt;
            sScene.camera.position.z +=
                    std::sin(sScene.boatXZAngle + (float) M_PI_2) * sScene.boatMovementPerSecond * dt;
        }
    } else if (sInput.buttonPressed[1]) {  // 's' pressed
        sScene.boatPosition.x -= std::sin(sScene.boatXZAngle + (float) M_PI_2) * sScene.boatMovementPerSecond * dt;
        sScene.boatPosition.z -= std::cos(sScene.boatXZAngle + (float) M_PI_2) * sScene.boatMovementPerSecond * dt;
        if (sScene.cameraMode == CameraMode::ThirdPersonView) {
            sScene.camera.position.x -=
                    std::sin(sScene.boatXZAngle + (float) M_PI_2) * sScene.boatMovementPerSecond * dt;
            sScene.camera.position.z -=
                    std::sin(sScene.boatXZAngle + (float) M_PI_2) * sScene.boatMovementPerSecond * dt;
        }
    }

    /* apply boat rotation */
    sScene.boatTransformationMatrix = Matrix4D::rotationY(sScene.boatXZAngle);

    /* apply boat position */
    sScene.boatTransformationMatrix =
            Matrix4D::translation(sScene.boatPosition) * sScene.boatTransformationMatrix;

    /* if in ThirdPersonView: update camera lookAt*/
    if (sScene.cameraMode == CameraMode::ThirdPersonView) {
        sScene.camera.lookAt = sScene.boatPosition;
    }

    /* if '1' or '2' is pressed, change the camera mode*/
    if (sInput.buttonPressed[4]) {
        sScene.cameraMode = CameraMode::FixedView;
    } else if (sInput.buttonPressed[5]) {
        sScene.cameraMode = CameraMode::ThirdPersonView;
    }


}

/* function to draw all objects in the scene */
void sceneDraw() {
    /* clear framebuffer color */
    glClearColor(135.0 / 255, 206.0 / 255, 235.0 / 255, 1.0);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    /*------------ render scene -------------*/
    /* use shader and set the uniforms (names match the ones in the shader) */
    {
        glUseProgram(sScene.shaderColor.id);
        shaderUniform(sScene.shaderColor, "uProj", cameraProjection(sScene.camera));
        shaderUniform(sScene.shaderColor, "uView", cameraView(sScene.camera));

        /* draw water plane */
        shaderUniform(sScene.shaderColor, "uModel", sScene.waterModelMatrix);
        glBindVertexArray(sScene.water.mesh.vao);
        glDrawElements(GL_TRIANGLES, sScene.water.mesh.size_ibo, GL_UNSIGNED_INT, nullptr);

        /* draw boat, requires to calculate the final model matrix from all transformations */
        shaderUniform(sScene.shaderColor, "uModel",
                      sScene.boatTranslationMatrix * sScene.boatTransformationMatrix * sScene.boatScalingMatrix);
        glBindVertexArray(sScene.boatMesh.vao);
        glDrawElements(GL_TRIANGLES, sScene.boatMesh.size_ibo, GL_UNSIGNED_INT, nullptr);
    }
    glCheckError();

    /* cleanup opengl state */
    glBindVertexArray(0);
    glUseProgram(0);
}

int main(int argc, char **argv) {
    /* create window/context */
    int width = 1280;
    int height = 720;
    GLFWwindow *window = windowCreate("Assignment 1 - Transformations, User Input and Camera", width, height);
    if (!window) { return EXIT_FAILURE; }

    /* set window callbacks */
    glfwSetKeyCallback(window, keyCallback);
    glfwSetCursorPosCallback(window, mousePosCallback);
    glfwSetMouseButtonCallback(window, mouseButtonCallback);
    glfwSetScrollCallback(window, mouseScrollCallback);
    glfwSetFramebufferSizeCallback(window, windowResizeCallback);


    /*---------- init opengl stuff ------------*/
    glEnable(GL_DEPTH_TEST);

    /* setup scene */
    sceneInit(width, height);

    /*-------------- main loop ----------------*/
    double timeStamp = glfwGetTime();
    double timeStampNew = 0.0;

    /* loop until user closes window */
    while (!glfwWindowShouldClose(window)) {
        /* poll and process input and window events */
        glfwPollEvents();

        /* update model matrix of boat */
        timeStampNew = glfwGetTime();
        sScene.waterSim.accumTime += timeStampNew - timeStamp;
        sceneUpdate(timeStampNew - timeStamp);
        timeStamp = timeStampNew;

        /* draw all objects in the scene */
        sceneDraw();

        /* swap front and back buffer */
        glfwSwapBuffers(window);
    }


    /*-------- cleanup --------*/
    /* delete opengl shader and buffers */
    shaderDelete(sScene.shaderColor);
    waterDelete(sScene.water);
    meshDelete(sScene.boatMesh);

    /* cleanup glfw/glcontext */
    windowDelete(window);

    return EXIT_SUCCESS;
}
