#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"

#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <vector>
#include <iostream>
#include <cstdlib> 
#include <cmath>   


const unsigned int WIDTH = 900;
const unsigned int HEIGHT = 900;
int CIRCLE_SEGMENTS = 100;
float CIRCLE_RADIUS = 0.05f; 
float GRAVITY = -0.1f; 
float TIME_STEP = 0.09f; 
float BOUNCE_DAMPING = 1.0f; 
float COLLISION_DAMPING = 0.9f; // damping factor for collisions between balls
float FRICTION = 0.97f; // damping factor for velocity (0.0 to 1.0)
bool isPaused = false;
bool energyLossEnabled = true;


struct Circle {
    glm::vec2 position;
    glm::vec2 velocity;
    glm::vec3 color;
};


std::vector<Circle> circles;
float aspectRatio = WIDTH / static_cast<float>(HEIGHT);


const char* vertexShaderSource = R"(
    #version 330 core
    layout(location = 0) in vec2 aPos;
    uniform mat4 model;
    void main() {
        gl_Position = model * vec4(aPos, 0.0, 1.0);
    }
)";

const char* fragmentShaderSource = R"(
    #version 330 core
    out vec4 FragColor;
    uniform vec3 color;
    void main() {
        FragColor = vec4(color, 1.0);
    }
)";

void framebuffer_size_callback(GLFWwindow* window, int width, int height);
void mouse_button_callback(GLFWwindow* window, int button, int action, int mods);
void processInput(GLFWwindow* window);

GLuint compileShader(GLenum type, const char* source);
GLuint createShaderProgram(const char* vertexSource, const char* fragmentSource);

void setupCircleVertexData(GLuint& VAO, GLuint& VBO);
void updateCircles();
bool checkCollision(const Circle& a, const Circle& b);
void resolveCollision(Circle& a, Circle& b);


int main() {
    if (!glfwInit()) {
        std::cerr << "Failed to initialize GLFW" << std::endl;
        return -1;
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    GLFWwindow* window = glfwCreateWindow(WIDTH, HEIGHT, "b", NULL, NULL);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      
    ImGui::StyleColorsDark();

    if (!window) {
        std::cerr << "Failed to create GLFW window" << std::endl;
        glfwTerminate();
        return -1;
    }

    glfwMakeContextCurrent(window);
    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);
    glfwSetMouseButtonCallback(window, mouse_button_callback);

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        std::cerr << "Failed to initialize GLAD" << std::endl;
        return -1;
    }

    GLuint shaderProgram = createShaderProgram(vertexShaderSource, fragmentShaderSource);

    //circle vertex data
    GLuint circleVAO, circleVBO;
    setupCircleVertexData(circleVAO, circleVBO);

    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 130");

    ImVec4 background_color = ImVec4(0.45f, 0.55f, 0.6f, 1.0f);

    glfwSwapInterval(1);

    while (!glfwWindowShouldClose(window)) {
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        processInput(window);

        glClear(GL_COLOR_BUFFER_BIT);

        glUseProgram(shaderProgram);

        GLint modelLoc = glGetUniformLocation(shaderProgram, "model");
        GLint colorLoc = glGetUniformLocation(shaderProgram, "color");

        // draw circles
        glBindVertexArray(circleVAO);
        for (const auto& circle : circles) {
            glm::mat4 model = glm::translate(glm::mat4(1.0f), glm::vec3(circle.position, 0.0f));
            model = glm::scale(model, glm::vec3(CIRCLE_RADIUS * aspectRatio, CIRCLE_RADIUS, 1.0f));
            glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(model));
            glUniform3f(colorLoc, circle.color.r, circle.color.g, circle.color.b);
            glDrawArrays(GL_TRIANGLE_FAN, 0, CIRCLE_SEGMENTS + 2);
        }
        glBindVertexArray(0);


        glfwPollEvents();

        ImVec2 initialWindowSize(400, 350);
        ImGui::SetNextWindowSize(initialWindowSize, ImGuiCond_FirstUseEver);
        ImGui::Begin("Properties");;
        ImGui::SliderFloat("radius", &CIRCLE_RADIUS, 0.02f, 1.0f);
        ImGui::SliderFloat("friction", &FRICTION, 0.1f, 0.97f);
        ImGui::SliderFloat("bounce damping", &BOUNCE_DAMPING, 0.01f, 2.0f);
        ImGui::SliderFloat("gravity", &GRAVITY, -10.0f, 10.0f);
        ImGui::Checkbox("Energy Loss Enabled", &energyLossEnabled);
        ImGui::Checkbox("Pause Simulation", &isPaused);
        ImGui::ColorEdit3("background", (float*)&background_color);
        if (!isPaused) {
            updateCircles();
        }
        if (ImGui::Button("Clear Screen")) {
            circles.clear(); 
        }
        ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 100.0f / io.Framerate, io.Framerate);
        ImGui::End();

        ImGui::Render();
        glClearColor(background_color.x * background_color.w, background_color.y * background_color.w, background_color.z * background_color.w, background_color.w);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        glfwSwapBuffers(window);
    }

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    glDeleteVertexArrays(1, &circleVAO);
    glDeleteBuffers(1, &circleVBO);
    glDeleteProgram(shaderProgram);
    glfwTerminate();
    return 0;
}

void framebuffer_size_callback(GLFWwindow* window, int width, int height) {
    glViewport(0, 0, width, height);
    aspectRatio = width / static_cast<float>(height);
}

void mouse_button_callback(GLFWwindow* window, int button, int action, int mods) {
    if (action == GLFW_PRESS) {
        double xpos, ypos;
        glfwGetCursorPos(window, &xpos, &ypos);

        // convert mouse pos to normalized screen coords
        float x = (xpos / WIDTH) * 2.0f - 1.0f;
        float y = 1.0f - (ypos / HEIGHT) * 2.0f;


        glm::vec3 color(
            static_cast<float>(rand()) / 256,
            static_cast<float>(rand()) / RAND_MAX,
            static_cast<float>(rand()) / RAND_MAX
        );

        if (button == GLFW_MOUSE_BUTTON_LEFT) {
            //random initial velocity
            float speed = static_cast<float>(rand()) / RAND_MAX * 0.35f; // Speed between 0 and 0.5
            float angle = static_cast<float>(rand()) / RAND_MAX * 1.0f * glm::pi<float>(); // Random angle

            glm::vec2 velocity(
                speed * cos(angle), // x-component of velocity
                speed * sin(angle)  // y-component of velocity
            );

            // add circle at mouse position
            circles.push_back(Circle{ glm::vec2(x, y), velocity, color });
        }
        else if (button == GLFW_MOUSE_BUTTON_RIGHT) {
            // remove circle closest to mouse position
            auto it = std::remove_if(circles.begin(), circles.end(), [x, y](const Circle& circle) {
                return glm::distance(circle.position, glm::vec2(x, y)) < CIRCLE_RADIUS;
                });
            circles.erase(it, circles.end());
        }
    }
}


void processInput(GLFWwindow* window) {
    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS) {
        glfwSetWindowShouldClose(window, true);
    }
}

void updateCircles() {
    for (auto& circle : circles) {
        // only update velocity based on gravity if gravity is non-zero
        if (GRAVITY != 0.0f) {
            circle.velocity.y += GRAVITY * TIME_STEP;
        }
     
        circle.position += circle.velocity * TIME_STEP;

        // apply friction to reduce velocity gradually only if energy loss is enabled
        if (energyLossEnabled) {
            circle.velocity *= FRICTION;
        }

        // check boundary collision
        if (circle.position.x - CIRCLE_RADIUS < -1.0f) {
            circle.position.x = -1.0f + CIRCLE_RADIUS;
            circle.velocity.x *= -BOUNCE_DAMPING;
        }
        if (circle.position.x + CIRCLE_RADIUS > 1.0f) {
            circle.position.x = 1.0f - CIRCLE_RADIUS;
            circle.velocity.x *= -BOUNCE_DAMPING;
        }
        if (circle.position.y - CIRCLE_RADIUS < -1.0f) {
            circle.position.y = -1.0f + CIRCLE_RADIUS;
            circle.velocity.y *= -BOUNCE_DAMPING;
        }
        if (circle.position.y + CIRCLE_RADIUS > 1.0f) {
            circle.position.y = 1.0f - CIRCLE_RADIUS;
            circle.velocity.y *= -BOUNCE_DAMPING;
        }
    }

    // resolve collisions if any
    const int collisionIterations = 10;
    for (int iter = 0; iter < collisionIterations; ++iter) {
        for (size_t i = 0; i < circles.size(); ++i) {
            for (size_t j = i + 1; j < circles.size(); ++j) {
                if (checkCollision(circles[i], circles[j])) {
                    resolveCollision(circles[i], circles[j]);
                }
            }
        }
    }
}


bool checkCollision(const Circle& a, const Circle& b) {
    float distance = glm::length(a.position - b.position);
    return distance < 2 * CIRCLE_RADIUS;
}


void resolveCollision(Circle& a, Circle& b) {
    glm::vec2 collisionNormal = b.position - a.position;
    float distance = glm::length(collisionNormal);

    if (distance == 0.0f) return; // avoid division by zero if circles are exactly on the same spot

    collisionNormal /= distance; // normalize the collision normal

    // calculate the overlap if any
    float overlap = 2 * CIRCLE_RADIUS - distance;

    // adjust positions to ensure no overlap
    glm::vec2 correction = 0.9f * overlap * collisionNormal;
    a.position -= correction;
    b.position += correction;

    // calculate relative velocity
    glm::vec2 relativeVelocity = b.velocity - a.velocity;

    // calculate the impulse magnitude
    float impulse = glm::dot(relativeVelocity, collisionNormal);

    // apply a fraction of the impulse to reduce velocities only if energy loss is enabled
    if (energyLossEnabled) {
        float dampingFactor = 0.2f; // fraction of the impulse to apply
        glm::vec2 impulseVector = 2 * dampingFactor * impulse * collisionNormal;

        a.velocity += impulseVector;
        b.velocity -= impulseVector;
    }
}




GLuint compileShader(GLenum type, const char* source) {
    GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &source, NULL);
    glCompileShader(shader);

    GLint success;
    GLchar infoLog[512];
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);

    return shader;
}

GLuint createShaderProgram(const char* vertexSource, const char* fragmentSource) {
    GLuint vertexShader = compileShader(GL_VERTEX_SHADER, vertexSource);
    GLuint fragmentShader = compileShader(GL_FRAGMENT_SHADER, fragmentSource);

    GLuint shaderProgram = glCreateProgram();
    glAttachShader(shaderProgram, vertexShader);
    glAttachShader(shaderProgram, fragmentShader);
    glLinkProgram(shaderProgram);

    GLint success;
    GLchar infoLog[512];
    glGetProgramiv(shaderProgram, GL_LINK_STATUS, &success);
    if (!success) {
        glGetProgramInfoLog(shaderProgram, 512, NULL, infoLog);
        std::cerr << "Shader program linking failed\n" << infoLog << std::endl;
    }

    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);

    return shaderProgram;
}

void setupCircleVertexData(GLuint& VAO, GLuint& VBO) {
    // generate circle vertex data
    std::vector<float> vertices;
    float angleStep = 2.0f * glm::pi<float>() / CIRCLE_SEGMENTS;

    vertices.push_back(0.0f); // centre
    vertices.push_back(0.0f);

    for (unsigned int i = 0; i <= CIRCLE_SEGMENTS; ++i) {
        float angle = i * angleStep;
        vertices.push_back(cos(angle));
        vertices.push_back(sin(angle));
    }

    GLuint VBOtemp, VAOtemp;
    glGenVertexArrays(1, &VAOtemp);
    glGenBuffers(1, &VBOtemp);

    glBindVertexArray(VAOtemp);

    glBindBuffer(GL_ARRAY_BUFFER, VBOtemp);
    glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(float), vertices.data(), GL_STATIC_DRAW);

    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
    //glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);         //uncomment for the balls to be created with a cool design

    VAO = VAOtemp;
    VBO = VBOtemp;
}