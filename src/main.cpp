#include "glad.h"
#include "glfw3.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <random>
#include <string>
#include <vector>

namespace
{
constexpr int screenWidth = 800;
constexpr int screenHeight = 600;
constexpr float roadLeft = 190.0f;
constexpr float roadRight = 610.0f;
constexpr float carWidth = 62.0f;
constexpr float carHeight = 104.0f;

struct Vertex
{
    float x;
    float y;
    float r;
    float g;
    float b;
};

struct Obstacle
{
    float x;
    float y;
};

const char* vertexShaderSource =
    "#version 330 core\n"
    "layout (location = 0) in vec2 aPos;\n"
    "layout (location = 1) in vec3 aColor;\n"
    "out vec3 vertexColor;\n"
    "void main()\n"
    "{\n"
    "    gl_Position = vec4(aPos.x / 400.0 - 1.0, 1.0 - aPos.y / 300.0, 0.0, 1.0);\n"
    "    vertexColor = aColor;\n"
    "}\n";

const char* fragmentShaderSource =
    "#version 330 core\n"
    "in vec3 vertexColor;\n"
    "out vec4 FragColor;\n"
    "void main()\n"
    "{\n"
    "    FragColor = vec4(vertexColor, 1.0);\n"
    "}\n";

void framebufferSizeCallback(GLFWwindow* window, int width, int height)
{
    (void)window;
    if (width <= 0 || height <= 0)
        return;
    const float targetAspect = static_cast<float>(screenWidth) / screenHeight;
    const float windowAspect = static_cast<float>(width) / height;
    int viewportWidth = width;
    int viewportHeight = height;

    if (windowAspect > targetAspect)
        viewportWidth = static_cast<int>(height * targetAspect);
    else
        viewportHeight = static_cast<int>(width / targetAspect);

    glViewport((width - viewportWidth) / 2, (height - viewportHeight) / 2,
               viewportWidth, viewportHeight);
}

unsigned int createShader(unsigned int type, const char* source)
{
    const unsigned int shader = glCreateShader(type);
    glShaderSource(shader, 1, &source, nullptr);
    glCompileShader(shader);

    int success = GL_FALSE;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success)
    {
        char infoLog[512];
        glGetShaderInfoLog(shader, sizeof(infoLog), nullptr, infoLog);
        std::cerr << "Shader compilation failed:\n" << infoLog << std::endl;
        glDeleteShader(shader);
        return 0;
    }
    return shader;
}

void addRectangle(std::vector<Vertex>& vertices, float x, float y, float width, float height,
                  float red, float green, float blue)
{
    const Vertex topLeft{x, y, red, green, blue};
    const Vertex topRight{x + width, y, red, green, blue};
    const Vertex bottomRight{x + width, y + height, red, green, blue};
    const Vertex bottomLeft{x, y + height, red, green, blue};
    vertices.insert(vertices.end(), {topLeft, topRight, bottomRight,
                                     topLeft, bottomRight, bottomLeft});
}

std::array<std::uint8_t, 7> glyph(char character)
{
    switch (character)
    {
    case 'A': return {14, 17, 17, 31, 17, 17, 17};
    case 'D': return {30, 17, 17, 17, 17, 17, 30};
    case 'E': return {31, 16, 16, 30, 16, 16, 31};
    case 'G': return {14, 17, 16, 23, 17, 17, 15};
    case 'M': return {17, 27, 21, 21, 17, 17, 17};
    case 'O': return {14, 17, 17, 17, 17, 17, 14};
    case 'P': return {30, 17, 17, 30, 16, 16, 16};
    case 'R': return {30, 17, 17, 30, 20, 18, 17};
    case 'S': return {15, 16, 16, 14, 1, 1, 30};
    case 'T': return {31, 4, 4, 4, 4, 4, 4};
    case 'V': return {17, 17, 17, 17, 17, 10, 4};
    case 'W': return {17, 17, 17, 21, 21, 21, 10};
    case '0': return {14, 17, 19, 21, 25, 17, 14};
    case '1': return {4, 12, 4, 4, 4, 4, 14};
    case '2': return {14, 17, 1, 2, 4, 8, 31};
    case '3': return {30, 1, 1, 14, 1, 1, 30};
    case '4': return {2, 6, 10, 18, 31, 2, 2};
    case '5': return {31, 16, 16, 30, 1, 1, 30};
    case '6': return {14, 16, 16, 30, 17, 17, 14};
    case '7': return {31, 1, 2, 4, 8, 8, 8};
    case '8': return {14, 17, 17, 14, 17, 17, 14};
    case '9': return {14, 17, 17, 15, 1, 1, 14};
    case ':': return {0, 4, 4, 0, 4, 4, 0};
    default: return {0, 0, 0, 0, 0, 0, 0};
    }
}

void addText(std::vector<Vertex>& vertices, const std::string& text, float x, float y,
             float scale, float red = 1.0f, float green = 1.0f, float blue = 1.0f)
{
    float cursorX = x;
    for (char character : text)
    {
        const auto pixels = glyph(character);
        for (int row = 0; row < 7; ++row)
        {
            for (int column = 0; column < 5; ++column)
            {
                if ((pixels[row] & (1u << (4 - column))) != 0)
                    addRectangle(vertices, cursorX + column * scale, y + row * scale,
                                 scale, scale, red, green, blue);
            }
        }
        cursorX += 6.0f * scale;
    }
}

void addCar(std::vector<Vertex>& vertices, float centerX, float topY,
            float red, float green, float blue, bool player)
{
    const float left = centerX - carWidth / 2.0f;
    const float dark = 0.12f;
    addRectangle(vertices, left - 6, topY + 18, 12, 27, dark, dark, dark);
    addRectangle(vertices, left - 6, topY + 62, 12, 27, dark, dark, dark);
    addRectangle(vertices, left + carWidth - 6, topY + 18, 12, 27, dark, dark, dark);
    addRectangle(vertices, left + carWidth - 6, topY + 62, 12, 27, dark, dark, dark);
    addRectangle(vertices, left, topY, carWidth, carHeight, red, green, blue);
    addRectangle(vertices, left + 9, topY + 16, carWidth - 18, 25, 0.12f, 0.20f, 0.25f);
    addRectangle(vertices, left + 12, topY + 48, carWidth - 24, 28,
                 red * 0.72f, green * 0.72f, blue * 0.72f);
    addRectangle(vertices, left + 8, topY + 6, 10, 5, 1.0f, 0.92f, 0.50f);
    addRectangle(vertices, left + carWidth - 18, topY + 6, 10, 5, 1.0f, 0.92f, 0.50f);
    addRectangle(vertices, left + 8, topY + carHeight - 8, 12, 5,
                 player ? 0.10f : 0.95f, 0.12f, 0.12f);
    addRectangle(vertices, left + carWidth - 20, topY + carHeight - 8, 12, 5,
                 player ? 0.10f : 0.95f, 0.12f, 0.12f);
}

void resetGame(float& playerX, float& roadOffset, float& elapsed, float& spawnTimer,
               bool& gameOver, std::vector<Obstacle>& obstacles, int& previousLane)
{
    playerX = screenWidth / 2.0f;
    roadOffset = 0.0f;
    elapsed = 0.0f;
    spawnTimer = 0.65f;
    gameOver = false;
    previousLane = -1;
    obstacles.clear();
}
}

int main()
{
    if (!glfwInit())
    {
        std::cerr << "Failed to initialize GLFW." << std::endl;
        return EXIT_FAILURE;
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

#ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif

    GLFWwindow* window = glfwCreateWindow(screenWidth, screenHeight, "Car Dodge - Score: 0",
                                          nullptr, nullptr);
    if (window == nullptr)
    {
        std::cerr << "Failed to create GLFW window." << std::endl;
        glfwTerminate();
        return EXIT_FAILURE;
    }

    glfwMakeContextCurrent(window);
    glfwSetFramebufferSizeCallback(window, framebufferSizeCallback);
    glfwSwapInterval(1);

    if (!gladLoadGLLoader(reinterpret_cast<GLADloadproc>(glfwGetProcAddress)))
    {
        std::cerr << "Failed to initialize GLAD." << std::endl;
        glfwDestroyWindow(window);
        glfwTerminate();
        return EXIT_FAILURE;
    }
    int framebufferWidth;
    int framebufferHeight;
    glfwGetFramebufferSize(window, &framebufferWidth, &framebufferHeight);
    framebufferSizeCallback(window, framebufferWidth, framebufferHeight);

    const unsigned int vertexShader = createShader(GL_VERTEX_SHADER, vertexShaderSource);
    const unsigned int fragmentShader = createShader(GL_FRAGMENT_SHADER, fragmentShaderSource);
    if (vertexShader == 0 || fragmentShader == 0)
    {
        if (vertexShader != 0)
            glDeleteShader(vertexShader);
        if (fragmentShader != 0)
            glDeleteShader(fragmentShader);
        glfwDestroyWindow(window);
        glfwTerminate();
        return EXIT_FAILURE;
    }

    const unsigned int shaderProgram = glCreateProgram();
    glAttachShader(shaderProgram, vertexShader);
    glAttachShader(shaderProgram, fragmentShader);
    glLinkProgram(shaderProgram);
    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);
    int linkSuccess = GL_FALSE;
    glGetProgramiv(shaderProgram, GL_LINK_STATUS, &linkSuccess);
    if (!linkSuccess)
    {
        char infoLog[512];
        glGetProgramInfoLog(shaderProgram, sizeof(infoLog), nullptr, infoLog);
        std::cerr << "Shader program linking failed:\n" << infoLog << std::endl;
        glDeleteProgram(shaderProgram);
        glfwDestroyWindow(window);
        glfwTerminate();
        return EXIT_FAILURE;
    }

    unsigned int vertexArray;
    unsigned int vertexBuffer;
    glGenVertexArrays(1, &vertexArray);
    glGenBuffers(1, &vertexBuffer);
    glBindVertexArray(vertexArray);
    glBindBuffer(GL_ARRAY_BUFFER, vertexBuffer);
    glBufferData(GL_ARRAY_BUFFER, 0, nullptr, GL_DYNAMIC_DRAW);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex),
                          reinterpret_cast<void*>(offsetof(Vertex, x)));
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex),
                          reinterpret_cast<void*>(offsetof(Vertex, r)));
    glEnableVertexAttribArray(1);
    glBindVertexArray(0);

    const std::array<float, 3> laneCenters = {260.0f, 400.0f, 540.0f};
    std::mt19937 randomEngine(std::random_device{}());
    std::uniform_int_distribution<int> laneDistribution(0, 2);
    std::vector<Obstacle> obstacles;
    float playerX;
    float roadOffset;
    float elapsed;
    float spawnTimer;
    bool gameOver;
    int previousLane;
    resetGame(playerX, roadOffset, elapsed, spawnTimer, gameOver, obstacles, previousLane);

    double previousFrameTime = glfwGetTime();
    bool wasRestartPressed = false;
    std::string previousTitle;

    while (!glfwWindowShouldClose(window))
    {
        const double currentTime = glfwGetTime();
        const float deltaTime = std::min(static_cast<float>(currentTime - previousFrameTime), 0.05f);
        previousFrameTime = currentTime;
        glfwPollEvents();

        const bool restartPressed = glfwGetKey(window, GLFW_KEY_R) == GLFW_PRESS;
        if (gameOver && restartPressed && !wasRestartPressed)
            resetGame(playerX, roadOffset, elapsed, spawnTimer, gameOver, obstacles, previousLane);
        wasRestartPressed = restartPressed;

        if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
            glfwSetWindowShouldClose(window, true);

        if (!gameOver)
        {
            float steering = 0.0f;
            if (glfwGetKey(window, GLFW_KEY_LEFT) == GLFW_PRESS ||
                glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
                steering -= 1.0f;
            if (glfwGetKey(window, GLFW_KEY_RIGHT) == GLFW_PRESS ||
                glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
                steering += 1.0f;

            playerX += steering * 390.0f * deltaTime;
            playerX = std::max(roadLeft + carWidth / 2.0f + 10.0f,
                               std::min(playerX, roadRight - carWidth / 2.0f - 10.0f));
            elapsed += deltaTime;

            const float trafficSpeed = std::min(250.0f + elapsed * 3.0f, 470.0f);
            roadOffset = std::fmod(roadOffset + trafficSpeed * deltaTime, 120.0f);
            spawnTimer -= deltaTime;
            if (spawnTimer <= 0.0f)
            {
                int lane = laneDistribution(randomEngine);
                if (lane == previousLane)
                    lane = (lane + 1 + laneDistribution(randomEngine) % 2) % 3;
                previousLane = lane;
                obstacles.push_back({laneCenters[lane], -carHeight});
                spawnTimer = std::max(0.52f, 1.12f - elapsed * 0.002f);
            }

            for (Obstacle& obstacle : obstacles)
                obstacle.y += trafficSpeed * deltaTime;

            obstacles.erase(std::remove_if(obstacles.begin(), obstacles.end(),
                [](const Obstacle& obstacle) { return obstacle.y > screenHeight; }), obstacles.end());

            for (const Obstacle& obstacle : obstacles)
            {
                const bool overlapsX = std::abs(playerX - obstacle.x) < carWidth - 8.0f;
                const bool overlapsY = std::abs(448.0f - obstacle.y) < carHeight - 10.0f;
                if (overlapsX && overlapsY)
                {
                    gameOver = true;
                    break;
                }
            }
        }

        const int score = static_cast<int>(elapsed * 10.0f);
        const std::string title = gameOver
            ? "Car Dodge - Game Over - Score: " + std::to_string(score) + " - Press R"
            : "Car Dodge - Score: " + std::to_string(score);
        if (title != previousTitle)
        {
            glfwSetWindowTitle(window, title.c_str());
            previousTitle = title;
        }

        std::vector<Vertex> vertices;
        vertices.reserve(1800);
        addRectangle(vertices, 0, 0, screenWidth, screenHeight, 0.08f, 0.36f, 0.19f);
        addRectangle(vertices, roadLeft, 0, roadRight - roadLeft, screenHeight,
                     0.20f, 0.21f, 0.23f);
        addRectangle(vertices, roadLeft + 8, 0, 5, screenHeight, 0.92f, 0.88f, 0.61f);
        addRectangle(vertices, roadRight - 13, 0, 5, screenHeight, 0.92f, 0.88f, 0.61f);

        for (float laneLineX : {330.0f, 470.0f})
        {
            for (float y = -120.0f + roadOffset; y < screenHeight; y += 120.0f)
                addRectangle(vertices, laneLineX - 3, y, 6, 58, 0.83f, 0.83f, 0.78f);
        }

        for (const Obstacle& obstacle : obstacles)
            addCar(vertices, obstacle.x, obstacle.y, 0.84f, 0.16f, 0.12f, false);
        addCar(vertices, playerX, 448.0f, 0.12f, 0.53f, 0.96f, true);

        addRectangle(vertices, 20, 18, 170, 47, 0.08f, 0.10f, 0.12f);
        addText(vertices, "SCORE", 32, 27, 3.0f);
        const std::string scoreText = std::to_string(score);
        addText(vertices, scoreText, 124, 28, 3.0f, 1.0f, 0.84f, 0.24f);
        addText(vertices, "ARROWS A D TO MOVE", 245, 565, 2.0f, 0.94f, 0.94f, 0.94f);

        if (gameOver)
        {
            addRectangle(vertices, 190, 220, 420, 160, 0.10f, 0.10f, 0.12f);
            addText(vertices, "GAME OVER", 286, 246, 5.0f, 1.0f, 0.32f, 0.24f);
            addText(vertices, "SCORE", 306, 292, 3.0f);
            addText(vertices, scoreText, 416, 292, 3.0f, 1.0f, 0.84f, 0.24f);
            addText(vertices, "PRESS R TO RESTART", 260, 333, 2.0f);
        }

        glClearColor(0.04f, 0.12f, 0.08f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        glUseProgram(shaderProgram);
        glBindVertexArray(vertexArray);
        glBindBuffer(GL_ARRAY_BUFFER, vertexBuffer);
        glBufferData(GL_ARRAY_BUFFER,
                     static_cast<GLsizeiptr>(vertices.size() * sizeof(Vertex)),
                     vertices.data(), GL_DYNAMIC_DRAW);
        glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(vertices.size()));
        glfwSwapBuffers(window);
    }

    glDeleteVertexArrays(1, &vertexArray);
    glDeleteBuffers(1, &vertexBuffer);
    glDeleteProgram(shaderProgram);
    glfwDestroyWindow(window);
    glfwTerminate();
    return EXIT_SUCCESS;
}
