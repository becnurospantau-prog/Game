#include <glad/gl.h>
#include <GLFW/glfw3.h>

#include <cmath>
#include <iostream>

const int WIDTH  = 1280;
const int HEIGHT = 720;

bool whiteBackground = false;
bool spaceWasPressed = false;

bool wireframe = false;
bool wKeyWasPressed = false;

bool useLineLoop = false;
bool lKeyWasPressed = false;

// --- 2-АПТА: уақытша шейдерлер (3-аптада жақсартылады) ---
const char* vertexSrc = R"(
#version 330 core
layout (location = 0) in vec3 aPos;
void main() { gl_Position = vec4(aPos, 1.0); }
)";

const char* fragmentSrc = R"(
#version 330 core
out vec4 FragColor;
uniform vec3 uColor;
void main() { FragColor = vec4(uColor, 1.0); }
)";

void onResize(GLFWwindow*, int width, int height) {
    glViewport(0, 0, width, height);
}

void processInput(GLFWwindow* window) {
    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS) {
        glfwSetWindowShouldClose(window, true);
    }

    bool spacePressed = glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS;
    if (spacePressed && !spaceWasPressed) {
        whiteBackground = !whiteBackground;
    }
    spaceWasPressed = spacePressed;

    // W — wireframe қосу/өшіру (3-тапсырма)
    bool wPressed = glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS;
    if (wPressed && !wKeyWasPressed) {
        wireframe = !wireframe;
        glPolygonMode(GL_FRONT_AND_BACK, wireframe ? GL_LINE : GL_FILL);
    }
    wKeyWasPressed = wPressed;

    // L — GL_TRIANGLES / GL_LINE_LOOP ауыстыру (2-тапсырма)
    bool lPressed = glfwGetKey(window, GLFW_KEY_L) == GLFW_PRESS;
    if (lPressed && !lKeyWasPressed) {
        useLineLoop = !useLineLoop;
    }
    lKeyWasPressed = lPressed;
}

int main() {

    if (!glfwInit()) {
        std::cerr << "GLFW iske qosylmady\n";
        return -1;
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
#ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GLFW_TRUE);
#endif

    GLFWwindow* window = glfwCreateWindow(WIDTH, HEIGHT,
                                          "Kompyuterlik grafika",
                                          nullptr, nullptr);
    if (!window) {
        std::cerr << "Tereze zhasalmady.\n";
        glfwTerminate();
        return -1;
    }

    glfwMakeContextCurrent(window);
    glfwSetFramebufferSizeCallback(window, onResize);
    glfwSwapInterval(0);

    if (gladLoadGL(glfwGetProcAddress) == 0) {
        std::cerr << "GLAD zhuktelmedi\n";
        glfwTerminate();
        return -1;
    }

    std::cout << "OpenGL: " << glGetString(GL_VERSION) << "\n";
    std::cout << "GPU:    " << glGetString(GL_RENDERER) << "\n";

    // -----------------------------------------------------------------
    //  2-АПТА: вершина деректері — екі үшбұрыш (1-тапсырма), NDC-те
    // -----------------------------------------------------------------
    float vertices[] = {
        // 1-үшбұрыш (сол жақта)
        -0.9f, -0.5f, 0.0f,
        -0.3f, -0.5f, 0.0f,
        -0.6f,  0.5f, 0.0f,
        // 2-үшбұрыш (оң жақта)
         0.3f, -0.5f, 0.0f,
         0.9f, -0.5f, 0.0f,
         0.6f,  0.5f, 0.0f
    };

    unsigned int vao, vbo;
    glGenVertexArrays(1, &vao);
    glGenBuffers(1, &vbo);

    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    glBindVertexArray(0);

    // -----------------------------------------------------------------
    //  3-АПТА (уақытша нұсқасы): шейдерлерді компиляциялау
    // -----------------------------------------------------------------
    unsigned int vs = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vs, 1, &vertexSrc, nullptr);
    glCompileShader(vs);

    unsigned int fs = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fs, 1, &fragmentSrc, nullptr);
    glCompileShader(fs);

    unsigned int shader = glCreateProgram();
    glAttachShader(shader, vs);
    glAttachShader(shader, fs);
    glLinkProgram(shader);
    glDeleteShader(vs);
    glDeleteShader(fs);

    int uColorLoc = glGetUniformLocation(shader, "uColor");

    // -----------------------------------------------------------------
    //  Негізгі цикл
    // -----------------------------------------------------------------
    while (!glfwWindowShouldClose(window)) {

        processInput(window);

        if (whiteBackground) {
            glClearColor(1.0f, 1.0f, 1.0f, 1.0f);
        } else {
            float t = (float)glfwGetTime();
            float r = (std::sin(t * 2.0f) + 1.0f) * 0.5f * 0.3f;
            float g = (std::sin(t * 1.3f) + 1.0f) * 0.5f * 0.3f;
            glClearColor(r, g, 0.35f, 1.0f);
        }
        glClear(GL_COLOR_BUFFER_BIT);

        glUseProgram(shader);
        glBindVertexArray(vao);

        GLenum mode = useLineLoop ? GL_LINE_LOOP : GL_TRIANGLES;

        // 1-үшбұрыш — қызғылт-сары (4-тапсырма: түс өзгертілген)
        glUniform3f(uColorLoc, 1.0f, 0.5f, 0.2f);
        glDrawArrays(mode, 0, 3);

        // 2-үшбұрыш — көгілдір (4-тапсырма: екінші түс)
        glUniform3f(uColorLoc, 0.2f, 0.6f, 1.0f);
        glDrawArrays(mode, 3, 3);

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    // -----------------------------------------------------------------
    //  Тазалау
    // -----------------------------------------------------------------
    glDeleteVertexArrays(1, &vao);
    glDeleteBuffers(1, &vbo);
    glDeleteProgram(shader);

    glfwTerminate();
    return 0;
}