// This example is heavily based on the tutorial at https://open.gl

// OpenGL Helpers to reduce the clutter
#include "Helpers.h"

#ifdef __APPLE__
#define GL_SILENCE_DEPRECATION
// GLFW is necessary to handle the OpenGL context
#include <GLFW/glfw3.h>
#else
// GLFW is necessary to handle the OpenGL context
#include <GLFW/glfw3.h>
#endif

// OpenGL Mathematics Library
#include <glm/glm.hpp> // glm::vec3
#include <glm/vec3.hpp> // glm::vec3
#include <glm/vec4.hpp> // glm::vec4
#include <glm/mat4x4.hpp> // glm::mat4
#include <glm/gtc/matrix_transform.hpp> // glm::translate, glm::rotate, glm::scale, glm::perspective

#include <glm/gtc/type_ptr.hpp> // glm::value_ptr

// Timer
#include <chrono>

// Algorithms 
#include <algorithm>
#include <utility>
#include <iterator>

// VertexBufferObject wrapper
VertexBufferObject VBO;
VertexBufferObject VBO_C;

// Contains the vertex positions
std::vector<glm::vec2> V(3);

// Contains the per-vertex color
std::vector<glm::vec3> C(3);

static const int WIN_WIDTH = 800;
static const int WIN_HEIGHT = 600;
static const char WIN_TITLE[] = "Triangle Soup Editor";
static const double ANIMATION_TIME = 5;
static const double ANIMATION_STEP = 0.01;


enum AppMode {
    NONE = 0,
    INSERTION,
    TRANSFORMATION,
    REMOVE,
    COLOR_VERTEX,
    ANIMATION
};

struct Vertex {
    glm::vec2 vertex;
    glm::vec3 color;

    Vertex(glm::vec2 vertex, glm::vec3 color)
        : vertex(vertex), color(color) { }
};

class Triangle {
private:
    std::vector<Vertex> vertices;
    bool complete;
    
public:
    glm::vec3 fillColor;
    glm::vec3 outlineColor;

    Triangle(glm::vec3 fillColor = glm::vec3(1.0f, 1.0f, 1.0f), glm::vec3 outlineColor = glm::vec3(0.0f, 0.0f, 0.0f)) {
        this->fillColor    = fillColor;
        this->outlineColor = outlineColor;
        complete           = false;
    }

    void addVertex(glm::vec2 v, glm::vec3 c=glm::vec3(1.0f, 1.0f, 1.0f)) {
        if (complete) return;
        vertices.push_back(Vertex(v, c));
        complete = (vertices.size() == 3);
    }

    bool isComplete() const { return complete; }

    const std::vector<Vertex>& getVertices() const { return vertices; }
    
    size_t size() const { return vertices.size(); }
    
    const Vertex& operator[](size_t i) const { return vertices[i]; }
    Vertex& operator[](size_t i) { return vertices[i]; }

    bool isInside(glm::vec2 P) const {
        if (!isComplete()) return false;

        glm::vec2 A = vertices[0].vertex;
        glm::vec2 B = vertices[1].vertex;
        glm::vec2 C = vertices[2].vertex;

        double w1 = (A.x * (C.y - A.y) + (P.y - A.y) * (C.x - A.x) - P.x * (C.y - A.y)) / ((B.y - A.y) * (C.x - A.x) - (B.x - A.x) * (C.y - A.y));
        double w2 = (P.y - A.y - w1 * (B.y - A.y) ) / (C.y - A.y);

        return w1 >= 0 && w2 >= 0 && (w1 + w2) <= 1;
    }

    void move(glm::vec2 delta) {
        if (!isComplete()) return;

        for (size_t i = 0; i < vertices.size(); ++i) {
            vertices[i].vertex += delta;
        }
    }

    glm::vec2 barycenter() const {
        return glm::vec2(
            (vertices[0].vertex.x + vertices[1].vertex.x + vertices[2].vertex.x) / 3.0,
            (vertices[0].vertex.y + vertices[1].vertex.y + vertices[2].vertex.y) / 3.0
        );
    }

    void rotate(double angle) {
        glm::vec2 Pc = barycenter();

        double theta = glm::radians(angle);

        for (size_t i = 0; i < vertices.size(); ++i) {
            glm::vec2 P0(vertices[i].vertex.x, vertices[i].vertex.y);
            double x1 = (P0.x - Pc.x) * cos(theta) - (P0.y - Pc.y) * sin(theta) + Pc.x;
            double y1 = (P0.x - Pc.x) * sin(theta) + (P0.y - Pc.y) * cos(theta) + Pc.y;

            vertices[i].vertex = glm::vec2(x1, y1);
        }
    }

    void scale(double factor) {
        if (factor <= 0.0) return;

        glm::vec2 Pc = barycenter();

        for (size_t i = 0; i < vertices.size(); ++i) {
            glm::vec2 v = (vertices[i].vertex - Pc);
            v *= factor;
            vertices[i].vertex = (v + Pc);
        }

    }
};

/////////////////////////////////////////
AppMode curMode = AppMode::INSERTION;
std::vector<Triangle> triangles;
Triangle* selectedTriangle = NULL;
Vertex* selectedVertex = NULL;
glm::vec2 touchPos;
float ZoomFactor = 1.0f;
float SceneOffsetX = 0.0f;
float SceneOffsetY = 0.0f;
Triangle restoreTriangle;
Triangle* animationStartTriangle;
Triangle* animationFinalTriangle;

int AnimationTimeout = ANIMATION_TIME;
std::vector<glm::vec2> AnimationDeltas(3);
// Contains the view transformation
glm::mat4 view;

bool DrawingsInProgress = false;
bool TranslationInProgress = false;
int AnimationInProgress = 0;
glm::vec3 SelectedColor(1.0f, 1.0f, 0.0f);

std::vector<glm::vec3> COLOURS = {
    glm::vec3(1.0f, 0.5f, 0.5f),
    glm::vec3(1.0f, 0.0f, 0.0f),
    glm::vec3(0.0f, 1.0f, 0.0f),
    glm::vec3(0.0f, 0.0f, 1.0f),
    glm::vec3(0.0f, 1.0f, 1.0f),    
    glm::vec3(1.0f, 0.1f, 0.3f),
    glm::vec3(1.0f, 0.3f, 0.1f),    
    glm::vec3(0.6f, 0.6f, 0.6f),
    glm::vec3(0.7f, 0.7f, 0.7f),    
    glm::vec3(0.2f, 0.2f, 0.2f)
};



void framebuffer_size_callback(GLFWwindow* window, int width, int height)
{
    glViewport(0, 0, width, height);
}


void handleInsertionMove(double xworld, double yworld) {

    if (DrawingsInProgress) {
        // Update last added point
        triangles.back()[triangles.back().size() - 1].vertex = glm::vec2(xworld, yworld);
    }
}

void handleInsertionClick(double xworld, double yworld) {

    printf("Mouse down\n");

    if (DrawingsInProgress) {
        // Finish triangle
        if (triangles.back().isComplete()) {
            DrawingsInProgress = false;
        }
        else {
            triangles.back().addVertex(glm::vec2(xworld, yworld));
        }

        return;
    }

    triangles.push_back(Triangle());
    // One real point
    triangles.back().addVertex(glm::vec2(xworld, yworld));
    // Second fake point for 'mouse move' event
    triangles.back().addVertex(glm::vec2(xworld, yworld));

    DrawingsInProgress = true;
}


void handleSelectionMove(double xworld, double yworld) {
    if (TranslationInProgress == false || selectedTriangle == NULL) return;

    glm::vec2 curPos(xworld, yworld);
    glm::vec2 delta = curPos - touchPos;

    touchPos = curPos;

    printf("(%lf, %lf)\n", touchPos.x, touchPos.y);
    printf("\t(%lf, %lf)\n", curPos.x, curPos.y);
    printf("Delta=(%lf, %lf)\n", delta.x, delta.y);

    selectedTriangle->move(delta);
}

void handleTranslationClick(double xworld, double yworld) {
    

    if (TranslationInProgress) {
        printf("RELEASE\n");
        TranslationInProgress = false;
        //selected = NULL;
        return;
    }
    printf("TOUCH\n");
    touchPos = glm::vec2(xworld, yworld);
    selectedTriangle = NULL;

    for (size_t i = 0; i < triangles.size(); ++i) {
        if (triangles[i].isInside(touchPos)) {
            selectedTriangle = &triangles[i];
        }
    }


    TranslationInProgress = true;
}

void handleRemoveClick(double xworld, double yworld) {
    for (size_t i = 0; i < triangles.size(); ++i) {
        if (triangles[i].isInside(glm::vec2(xworld, yworld))) {
            triangles.erase(triangles.begin() + i);
            return;
        }
    }
}


void mouse_pos_callback(GLFWwindow* window, double xpos, double ypos) {

    // Get the size of the window
    int width, height;
    glfwGetWindowSize(window, &width, &height);

    // Convert screen position to world coordinates
    double xworld = ((xpos / double(width)) * 2) - 1;
    double yworld = (((height - 1 - ypos) / double(height)) * 2) - 1; // NOTE: y axis is flipped in glfw
    
                                                                      // Convert screen position to world coordinates
    glm::vec4 p_screen(xpos, height - 1 - ypos, 0, 1);
    glm::vec4 p_canonical((p_screen.x / width) * 2 - 1, (p_screen.y / height) * 2 - 1, 0, 1);
    glm::vec4 p_world = glm::inverse(view) * p_canonical;
    xworld = p_world.x;
    yworld = p_world.y;

    switch (curMode)
    {
    case NONE:
        break;
    case INSERTION:
    {
        if (DrawingsInProgress)
            handleInsertionMove(xworld, yworld);
    }
    break;
    case TRANSFORMATION:
    {
        if (TranslationInProgress)
            handleSelectionMove(xworld, yworld);
    }
        break;
    default:
        break;
    }

    // Upload the change to the GPU
    VBO.update(V);
}

void handleSelectClosestVertex(double xworld, double yworld) {
    printf("Select closest\n");
    const double RADIUS = 0.1;
    double minLen = -1;
    glm::vec2 p(xworld, yworld);

    selectedVertex = NULL;
    int triagPos = -1;
    int vertexPos = -1;

    for (size_t i = 0; i < triangles.size(); ++i) {
        for (size_t j = 0; j < 3; ++j) {
            double dist = glm::distance(p, triangles[i][j].vertex);
            printf("dist=%lf (%d)\n", dist, dist <= RADIUS);
            if (dist <= RADIUS && (minLen == -1 || dist < minLen)) {
                minLen = dist;
                triagPos = i;
                vertexPos = j;
            }
        }
    }

    if (triagPos != -1 && vertexPos != -1) {
        selectedVertex = &triangles[triagPos][vertexPos];
        printf("CLosest point: (%lf, %lf)\n", selectedVertex->vertex.x, selectedVertex->vertex.y);
    } else {
        printf("Closest point not found\n");
    }
}

void runAnimation() {
    if (AnimationInProgress != 3 || AnimationTimeout <= 0)  return;

    // Move one start triangle to final
    for (int i = 0; i < 3; ++i) {
        (*animationStartTriangle)[i].vertex += AnimationDeltas[i];
    }
    AnimationTimeout -= ANIMATION_STEP;
    if (AnimationTimeout <= 0) {
        AnimationInProgress = 0;
        animationStartTriangle = animationFinalTriangle = NULL;
        printf("Animation complete\n");
    }
}

void handleAnimationClick(double xworld, double yworld) {
    if (AnimationInProgress >= 2) return;
    printf("Animation click, AnimationStatus=[%d]\n", AnimationInProgress);

    if (AnimationInProgress == 1) {
        animationFinalTriangle = NULL;
        for (size_t i = 0; i < triangles.size(); ++i) {
            if (triangles[i].isInside(glm::vec2(xworld, yworld))) {
                animationFinalTriangle = &triangles[i];
                printf("Final triangle found\n");
                if (animationStartTriangle && animationFinalTriangle) {
                    printf("prepare animation...\n");
                    // Save prev pos
                    restoreTriangle = *animationStartTriangle;
                    // Find delta for each point
                    for (int i = 0; i < 3; ++i) {
                        glm::vec2 ds = (animationFinalTriangle->getVertices()[i].vertex - animationStartTriangle->getVertices()[i].vertex);
                        ds /= (ANIMATION_TIME / ANIMATION_STEP);
                        AnimationDeltas[i] = ds;
                    }

                    AnimationInProgress = 2;
                    AnimationTimeout = (ANIMATION_TIME / ANIMATION_STEP) ;

                    return;
                }
                printf("One of the triangles is not set, restart\n");
                break;
            }
        }

        AnimationInProgress = 0;
        animationStartTriangle = animationFinalTriangle = NULL;
        return;
    }

    animationStartTriangle = NULL;
    for (size_t i = 0; i < triangles.size(); ++i) {
        if (triangles[i].isInside(glm::vec2(xworld, yworld))) {
            animationStartTriangle = &triangles[i];
            AnimationInProgress = 1;
            return;
        }
    }

    AnimationInProgress = 0;
}

void mouse_button_callback(GLFWwindow* window, int button, int action, int mods)
{
    // Get the position of the mouse in the window
    double xpos, ypos;
    glfwGetCursorPos(window, &xpos, &ypos);

    // Get the size of the window
    int width, height;
    glfwGetWindowSize(window, &width, &height);

    // Convert screen position to world coordinates
    double xworld = ((xpos / double(width)) * 2) - 1;
    double yworld = (((height - 1 - ypos) / double(height)) * 2) - 1; // NOTE: y axis is flipped in glfw

    glm::vec4 p_screen(xpos, height - 1 - ypos, 0, 1);
    glm::vec4 p_canonical((p_screen.x / width) * 2 - 1, (p_screen.y / height) * 2 - 1, 0, 1);
    glm::vec4 p_world = glm::inverse(view) * p_canonical;
    xworld = p_world.x;
    yworld = p_world.y;

    switch (curMode)
    {
    case NONE:
        break;
    case INSERTION:
    {
        printf("INSERTION\n");
        if (button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_PRESS) {
            handleInsertionClick(xworld, yworld);
        }

        break;
    }
    case TRANSFORMATION:
    {
        if (button == GLFW_MOUSE_BUTTON_LEFT && (action == GLFW_PRESS || action == GLFW_RELEASE)) {
            handleTranslationClick(xworld, yworld);
        }

        break;
    }
    case REMOVE:
    {
        if (button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_PRESS) {
            handleRemoveClick(xworld, yworld);
        }
        break;
    }
    case COLOR_VERTEX:
    {
        if (button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_PRESS) {
            handleSelectClosestVertex(xworld, yworld);
        }
        break;
    }
    case ANIMATION:
    {
        if (button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_PRESS) {
            handleAnimationClick(xworld, yworld);
        }
        break;
    }
    default:
        break;
    }

    // Upload the change to the GPU
    VBO.update(V);
}

// Reset prev state
void resetMode(AppMode mode) {
    if (mode == AppMode::INSERTION) {
        // Check if triangles contains incomplete triangle. Stop drawing mode (in case if it was enable)
        if (!triangles.empty() && (triangles.back().size() % 3 != 0 || DrawingsInProgress)) {
            triangles.pop_back();
            V.resize(triangles.size() * 3);
            DrawingsInProgress = false;
        }
        return;
    }

    if (mode == AppMode::TRANSFORMATION) {
        touchPos = glm::vec2(0, 0);
        selectedTriangle = NULL;
        TranslationInProgress = false;
        return;
    }


    if (mode == AppMode::REMOVE) {
        touchPos = glm::vec2(0, 0);
        selectedTriangle = NULL;
        return;
    }

    if (mode == AppMode::COLOR_VERTEX) {
        selectedVertex = NULL;
        return;
    }

    if (mode == AppMode::ANIMATION) {
        AnimationInProgress = 0;
        animationStartTriangle = animationFinalTriangle = NULL;
        return;
    }
}


void handleAnimation(GLFWwindow* window) {
    //animationStartTriangle
}

void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods)
{
    if (action != GLFW_RELEASE) return;
    // Update the position of the first vertex if the keys 1,2, or 3 are pressed
    switch (key)
    {
    case GLFW_KEY_F1:
    {
        if (curMode == AppMode::ANIMATION) return;
        resetMode(curMode);
        printf("[Animation mode]\n");
        printf("You need to select 'Start' triangle and 'Final' form triangle, and press SPACE to start animation\n");
        curMode = AppMode::ANIMATION;
        break;
    }
    case GLFW_KEY_SPACE:
    {
        if (curMode != AppMode::ANIMATION || animationStartTriangle == NULL || animationFinalTriangle == NULL) return;
        printf("Animation start/pause\n");
        AnimationInProgress = 3;
        break;
    }
    case  GLFW_KEY_I: 
    {
        if (curMode == AppMode::INSERTION) return;
        printf("[Insertion mode]\n");
        resetMode(curMode);
        curMode = AppMode::INSERTION;        
        break;
    }
    case GLFW_KEY_O:
    {
        if (curMode == AppMode::TRANSFORMATION) return;
        printf("[Transformation mode]\n");
        resetMode(curMode);
        curMode = AppMode::TRANSFORMATION;   
        break;
    }
    case GLFW_KEY_P:
    {
        if (curMode == AppMode::REMOVE) return;
        printf("[Remove mode]\n");
        resetMode(curMode);
        curMode = AppMode::REMOVE;
        break;
    }
    case GLFW_KEY_H:
    {
        if (curMode != AppMode::TRANSFORMATION || selectedTriangle == NULL) return;
        printf("Rotate clockwise\n");
        // Clockwise rotation
        selectedTriangle->rotate(10.0f);
        break;
    }

    case GLFW_KEY_J:
    {
        if (curMode != AppMode::TRANSFORMATION || selectedTriangle == NULL) return;
        printf("Rotate counter clockwise\n");
        // Counter clockwise rotation
        selectedTriangle->rotate(-10.0f);
        break;
    }
    case GLFW_KEY_K:
    {
        if (curMode != AppMode::TRANSFORMATION || selectedTriangle == NULL) return;
        printf("Scale up for 20%\n");
        // Scale up
        selectedTriangle->scale(1.25);
        break;
    }
    case GLFW_KEY_L:
    {
        if (curMode != AppMode::TRANSFORMATION || selectedTriangle == NULL) return;
        printf("Scale down for 20%\n");
        // Scale down
        selectedTriangle->scale(0.75);
        break;
    }
    case GLFW_KEY_C:
    {
        if (curMode == AppMode::COLOR_VERTEX) return;
        printf("[Color vertex mode\n");
        resetMode(curMode);
        curMode = AppMode::COLOR_VERTEX;
        break;
    }
    // Color vertex
    case GLFW_KEY_1:
    {
        if (curMode != AppMode::COLOR_VERTEX || selectedVertex == NULL) return;
        printf("SET COLOR 1\n");
        selectedVertex->color = COLOURS[0];
        break;
    }
    case GLFW_KEY_2:
    {
        if (curMode != AppMode::COLOR_VERTEX || selectedVertex == NULL) return;
        printf("SET COLOR 2\n");
        selectedVertex->color = COLOURS[1];
        break;
    }
    case GLFW_KEY_3:
    {
        if (curMode != AppMode::COLOR_VERTEX || selectedVertex == NULL) return;
        printf("SET COLOR 3\n");
        selectedVertex->color = COLOURS[2];
        break;
    }
    case GLFW_KEY_4:
    {
        if (curMode != AppMode::COLOR_VERTEX || selectedVertex == NULL) return;
        printf("SET COLOR 4\n");
        selectedVertex->color = COLOURS[3];
        break;
    }
    case GLFW_KEY_5:
    {
        if (curMode != AppMode::COLOR_VERTEX || selectedVertex == NULL) return;
        printf("SET COLOR 5\n");
        selectedVertex->color = COLOURS[4];
        break;
    }
    case GLFW_KEY_6:
    {
        if (curMode != AppMode::COLOR_VERTEX || selectedVertex == NULL) return;
        printf("SET COLOR 6\n");
        selectedVertex->color = COLOURS[5];
        break;
    }
    case GLFW_KEY_7:
    {
        if (curMode != AppMode::COLOR_VERTEX || selectedVertex == NULL) return;
        printf("SET COLOR 7\n");
        selectedVertex->color = COLOURS[6];
        break;
    }
    case GLFW_KEY_8:
    {
        if (curMode != AppMode::COLOR_VERTEX || selectedVertex == NULL) return;
        printf("SET COLOR 8\n");
        selectedVertex->color = COLOURS[7];
        break;
    }
    case GLFW_KEY_9:
    {
        if (curMode != AppMode::COLOR_VERTEX || selectedVertex == NULL) return;
        printf("SET COLOR 9\n");
        selectedVertex->color = COLOURS[8];
        break;
    }
    case GLFW_KEY_W:
    {
        // Move scene down
        SceneOffsetY -= 0.2f;
        break;
    }
    case GLFW_KEY_A:
    {
        // Move scene right
        SceneOffsetX += 0.2f;
        break;
    }
    case GLFW_KEY_S:
    {
        // Move scene up
        SceneOffsetY += 0.2f;
        break;
    }
    case GLFW_KEY_D:
    {
        // Move scene left
        SceneOffsetX -= 0.2f;
        break;
    }
    case GLFW_KEY_MINUS:
    {
        if (ZoomFactor > 0.4) {
            ZoomFactor -= 0.2;
            printf("[-]ZoomFactor (%lf)\n", ZoomFactor);
        }
        break;
    }
    case GLFW_KEY_EQUAL:
    {
        if (ZoomFactor <= 0.9) {
            ZoomFactor += 0.2;
            printf("[+]ZoomFactor (%lf)\n", ZoomFactor);
        }
        break;
    }
    default:
        break;
    }

    // Upload the change to the GPU
    VBO.update(V);
    VBO_C.update(C);
}

int main(void)
{
    GLFWwindow* window;

    // Initialize the library
    if (!glfwInit())
        return -1;

    // Activate supersampling
    glfwWindowHint(GLFW_SAMPLES, 8);

    // Ensure that we get at least a 3.2 context
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2);

    // On apple we have to load a core profile with forward compatibility
#ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif

    // Create a windowed mode window and its OpenGL context
    window = glfwCreateWindow(WIN_WIDTH, WIN_HEIGHT, WIN_TITLE, NULL, NULL);
    if (!window)
    {
        glfwTerminate();
        return -1;
    }

    // Make the window's context current
    glfwMakeContextCurrent(window);

#ifndef __APPLE__
    glewExperimental = true;
    GLenum err = glewInit();
    if (GLEW_OK != err)
    {
        /* Problem: glewInit failed, something is seriously wrong. */
        fprintf(stderr, "Error: %s\n", glewGetErrorString(err));
    }
    glGetError(); // pull and savely ignonre unhandled errors like GL_INVALID_ENUM
    fprintf(stdout, "Status: Using GLEW %s\n", glewGetString(GLEW_VERSION));
#endif

    int major, minor, rev;
    major = glfwGetWindowAttrib(window, GLFW_CONTEXT_VERSION_MAJOR);
    minor = glfwGetWindowAttrib(window, GLFW_CONTEXT_VERSION_MINOR);
    rev = glfwGetWindowAttrib(window, GLFW_CONTEXT_REVISION);
    printf("OpenGL version recieved: %d.%d.%d\n", major, minor, rev);
    printf("Supported OpenGL is %s\n", (const char*)glGetString(GL_VERSION));
    printf("Supported GLSL is %s\n", (const char*)glGetString(GL_SHADING_LANGUAGE_VERSION));

    // Initialize the VAO
    // A Vertex Array Object (or VAO) is an object that describes how the vertex
    // attributes are stored in a Vertex Buffer Object (or VBO). This means that
    // the VAO is not the actual object storing the vertex data,
    // but the descriptor of the vertex data.
    VertexArrayObject VAO;
    VAO.init();
    VAO.bind();

    // Initialize the VBO with the vertices data
    // A VBO is a data container that lives in the GPU memory
    VBO.init();

    V.resize(1);
    V[0] = glm::vec2(0, 0);
    VBO.update(V);

    // Second VBO for colors
    VBO_C.init();
    C.resize(1);
    C[0] = glm::vec3(1, 0, 0);

    VBO_C.update(C);

    // Initialize the OpenGL Program
    // A program controls the OpenGL pipeline and it must contains
    // at least a vertex shader and a fragment shader to be valid
    Program program;
    const GLchar* vertex_shader =
        "#version 150 core\n"
        "in vec2 position;"
        "in vec3 color;"
        "out vec3 f_color;"
        "uniform mat4 view;"
        "void main()"
        "{"
        "    gl_Position = view * vec4(position, 0.0, 1.0);"
        "    f_color = color; "
        "}";
    const GLchar* fragment_shader =
        "#version 150 core\n"
        "in vec3 f_color;"
        "out vec4 outColor;"
        "uniform vec3 triangleColor;"
        "uniform float useTriangleColor=0.0f;"
        "void main()"
        "{"
        "    if(useTriangleColor > 0) {"
        "        outColor = vec4(triangleColor, 1.0);"
        "    } else {"
        "        outColor = vec4(f_color, 1.0);"
        "    }"
        "}";



    // Compile the two shaders and upload the binary to the GPU
    // Note that we have to explicitly specify that the output "slot" called outColor
    // is the one that we want in the fragment buffer (and thus on screen)
    program.init(vertex_shader, fragment_shader, "outColor");
    program.bind();

    // The vertex shader wants the position of the vertices as an input.
    // The following line connects the VBO we defined above with the position "slot"
    // in the vertex shader
    program.bindVertexAttribArray("position", VBO);
    program.bindVertexAttribArray("color", VBO_C);

    // Save the current time --- it will be used to dynamically change the triangle color
    auto t_start = std::chrono::high_resolution_clock::now();

    // Register the keyboard callback
    glfwSetKeyCallback(window, key_callback);

    // Register the mouse callback
    glfwSetMouseButtonCallback(window, mouse_button_callback);
    glfwSetCursorPosCallback(window, mouse_pos_callback);

    // Update viewport
    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);

    float dt = 0, prev = 0;
    // Loop until the user closes the window
    while (!glfwWindowShouldClose(window))
    {
        // Bind your VAO (not necessary if you have only one)
        VAO.bind();

        // Bind your program
        program.bind();

        // Set the uniform value depending on the time difference
        auto t_now = std::chrono::high_resolution_clock::now();
        float time = std::chrono::duration_cast<std::chrono::duration<float>>(t_now - t_start).count();
        glUniform1f(program.uniform("useTriangleColor"), 0.0f);
        glUniform3f(program.uniform("triangleColor"), 0.0f, 0.0f, 0.0f);//(float)(sin(time * 4.0f) + 1.0f) / 2.0f, 0.0f, 0.0f);

        
        dt = (time - prev);
        //printf("%lf %lf\n", dt, time);
        

        if (dt >= ANIMATION_STEP) {
            dt = 0;
            prev = time;
            if (curMode == AppMode::ANIMATION && AnimationInProgress == 3 && AnimationTimeout > 0) {
                runAnimation();
            }
        }

        // Get size of the window
        int width, height;
        glfwGetWindowSize(window, &width, &height);
        float aspect_ratio = float(height) / float(width); // corresponds to the necessary width scaling


        view = glm::scale(glm::mat4(1.f), glm::vec3(aspect_ratio * ZoomFactor, ZoomFactor, 1.0f));
        view = glm::translate(view, glm::vec3(SceneOffsetX, SceneOffsetY, 0.0f));


        glUniformMatrix4fv(program.uniform("view"), 1, GL_FALSE, glm::value_ptr(view));

        // Clear the framebuffer
        glClearColor(0.5f, 0.5f, 0.5f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        if (V.size() < triangles.size() * 3) V.resize(triangles.size() * 3);
        if (C.size() < triangles.size() * 3) C.resize(triangles.size() * 3);

        for (size_t i = 0; i < triangles.size(); ++i) {
            glm::vec3 fill = triangles[i].fillColor;
            glm::vec3 outline = triangles[i].outlineColor;

            if (triangles[i].isComplete()) {
                V[i * 3 + 0] = triangles[i][0].vertex;
                V[i * 3 + 1] = triangles[i][1].vertex;
                V[i * 3 + 2] = triangles[i][2].vertex;

                C[i * 3 + 0] = triangles[i][0].color;
                C[i * 3 + 1] = triangles[i][1].color;
                C[i * 3 + 2] = triangles[i][2].color;

                VBO.update(V);
                VBO_C.update(C);
                
                glUniform3f(program.uniform("triangleColor"), fill.x, fill.y, fill.z);
                glDrawArrays(GL_TRIANGLES, i * 3, 3);

                if (selectedTriangle == &triangles[i] || animationStartTriangle == &triangles[i] || animationFinalTriangle == &triangles[i]) {
                    glUniform3f(program.uniform("triangleColor"), SelectedColor.x, SelectedColor.y, SelectedColor.z);
                    glLineWidth(3);
                } else {                    
                    glUniform3f(program.uniform("triangleColor"), outline.x, outline.y, outline.z);
                    glLineWidth(1);
                }
                
                glUniform1f(program.uniform("useTriangleColor"), 1.0f);
                glDrawArrays(GL_LINE_LOOP, i * 3, 3);
                glUniform1f(program.uniform("useTriangleColor"), 0.0f);
            } else {
                V[i * 3 + 0] = triangles[i][0].vertex;
                V[i * 3 + 1] = triangles[i][1].vertex;

                C[i * 3 + 0] = triangles[i][0].color;
                C[i * 3 + 1] = triangles[i][1].color;

                VBO.update(V);
                VBO_C.update(C);

                glDrawArrays(GL_LINES, i * 3, 2);
            }
            
        }

        // Swap front and back buffers
        glfwSwapBuffers(window);

        // Poll for and process events
        glfwPollEvents();
    }

    // Deallocate opengl memory
    program.free();
    VAO.free();
    VBO.free();
    VBO_C.free();

    // Deallocate glfw internals
    glfwTerminate();
    return 0;
}
