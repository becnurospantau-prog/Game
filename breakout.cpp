// =====================================================================
//  BREAKOUT (Arkanoid) — OpenGL 3.3, GLFW, GLAD, GLM
//
//  Басқару:  A / D  немесе  ← / →  — тақтайшаны жылжыту
//            Space — добты ұшыру       R — қайта бастау       ESC — шығу
//
//  Ереже:    Добпен барлық кірпішті сын. Қоңыр жиекті кірпіш 2 рет соққыны керек етеді.
//            Кірпіштен бонус түсуі мүмкін:  жасыл — тақтайша кеңейеді, қызғылт — +1 жан.
//            Доп төменге түсіп кетсе — 1 жан кетеді. Барлық кірпіш біткенде келесі деңгей.
//
//  Код құрылымы: 1) геометрия   2) World (ойын логикасы, GL-сіз)   3) main (енгізу + сызу)
// =====================================================================
#include <glad/gl.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <algorithm>
#include <cmath>
#include <iostream>
#include <random>
#include <string>
#include <vector>

using glm::vec2;
using glm::vec3;
using glm::vec4;

const int   WIDTH = 1280, HEIGHT = 720;
const float PI = 3.14159265f;
float aspect = (float)WIDTH / HEIGHT;

// Ойын майданының өлшемдері (әлем координаттары, биіктігі -1...1)
const float FW = 1.3f;                 // майданның жарты ені
const float TOP = 0.82f;               // төбенің биіктігі (үстінде HUD)
const float PADDLE_Y = -0.88f, PH = 0.025f;   // тақтайшаның орны және жарты биіктігі
const float BR = 0.03f;                // доптың радиусы
const int   COLS = 10, ROWS = 6;

void onResize(GLFWwindow*, int w, int h) {
    glViewport(0, 0, w, h);
    if (h > 0) aspect = (float)w / h;
}

// ---------------------------------------------------------------------
//  Шейдерлер: вершина түсі (RGBA) * uTint
// ---------------------------------------------------------------------
const char* vsSrc = R"(
#version 330 core
layout (location = 0) in vec2 aPos;
layout (location = 1) in vec4 aColor;
uniform mat4 uModel, uProj;
out vec4 vColor;
void main() { gl_Position = uProj * uModel * vec4(aPos, 0.0, 1.0); vColor = aColor; }
)";
const char* fsSrc = R"(
#version 330 core
in vec4 vColor;
uniform vec4 uTint;
out vec4 FragColor;
void main() { FragColor = vColor * uTint; }
)";

unsigned int makeShader() {
    auto compile = [](GLenum type, const char* src) {
        unsigned int id = glCreateShader(type);
        glShaderSource(id, 1, &src, nullptr);
        glCompileShader(id);
        int ok;
        glGetShaderiv(id, GL_COMPILE_STATUS, &ok);
        if (!ok) { char log[512]; glGetShaderInfoLog(id, 512, nullptr, log); std::cerr << log << "\n"; }
        return id;
    };
    unsigned int vs = compile(GL_VERTEX_SHADER, vsSrc), fs = compile(GL_FRAGMENT_SHADER, fsSrc);
    unsigned int prog = glCreateProgram();
    glAttachShader(prog, vs);
    glAttachShader(prog, fs);
    glLinkProgram(prog);
    glDeleteShader(vs);
    glDeleteShader(fs);
    return prog;
}

// ---------------------------------------------------------------------
//  1. ГЕОМЕТРИЯ: вершина = (x, y, r, g, b, a), VAO + VBO + EBO
// ---------------------------------------------------------------------
struct Mesh { unsigned int vao = 0, vbo = 0, ebo = 0; int count = 0; };

struct Builder {
    std::vector<float> v;
    std::vector<unsigned int> idx;
    unsigned int add(float x, float y, vec4 c) {
        v.insert(v.end(), { x, y, c.r, c.g, c.b, c.a });
        return (unsigned int)(v.size() / 6 - 1);
    }
    void tri(unsigned int a, unsigned int b, unsigned int c) { idx.insert(idx.end(), { a, b, c }); }
    // Дөңгелек: центр түсі + жиек түсі (градиент)
    void disc(int n, vec4 centerCol, vec4 edgeCol) {
        unsigned int c = add(0, 0, centerCol), prev = 0;
        for (int k = 0; k <= n; k++) {
            float a = 2 * PI * k / n;
            unsigned int p = add(std::cos(a), std::sin(a), edgeCol);
            if (k) tri(c, prev, p);
            prev = p;
        }
    }
    // Бірлік төртбұрыш: жоғарғы және төменгі түсі бөлек (тік градиент)
    void quad(vec4 top, vec4 bottom) {
        unsigned int a = add(-.5f, -.5f, bottom), b = add(.5f, -.5f, bottom);
        unsigned int c = add(.5f, .5f, top), d = add(-.5f, .5f, top);
        tri(a, b, c);
        tri(a, c, d);
    }
    Mesh upload() {
        Mesh m;
        m.count = (int)idx.size();
        glGenVertexArrays(1, &m.vao);
        glGenBuffers(1, &m.vbo);
        glGenBuffers(1, &m.ebo);
        glBindVertexArray(m.vao);                                   // VAO бірінші
        glBindBuffer(GL_ARRAY_BUFFER, m.vbo);
        glBufferData(GL_ARRAY_BUFFER, v.size() * sizeof(float), v.data(), GL_STATIC_DRAW);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m.ebo);               // EBO — VAO байланып тұрғанда
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, idx.size() * sizeof(unsigned int), idx.data(), GL_STATIC_DRAW);
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)0);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)(2 * sizeof(float)));
        glEnableVertexAttribArray(1);
        glBindVertexArray(0);
        return m;
    }
};

// 5x7 пиксельдік цифрлар (ұпай мен деңгей үшін)
const char* DIGITS[10][7] = {
    {".###.", "#...#", "#..##", "#.#.#", "##..#", "#...#", ".###."}, {"..#..", ".##..", "..#..", "..#..", "..#..", "..#..", ".###."},
    {".###.", "#...#", "....#", "...#.", "..#..", ".#...", "#####"}, {".###.", "#...#", "....#", "..##.", "....#", "#...#", ".###."},
    {"...#.", "..##.", ".#.#.", "#..#.", "#####", "...#.", "...#."}, {"#####", "#....", "####.", "....#", "....#", "#...#", ".###."},
    {"..##.", ".#...", "#....", "####.", "#...#", "#...#", ".###."}, {"#####", "....#", "...#.", "..#..", ".#...", ".#...", ".#..."},
    {".###.", "#...#", "#...#", ".###.", "#...#", "#...#", ".###."}, {".###.", "#...#", "#...#", ".####", "....#", "...#.", ".##.."},
};

// ---------------------------------------------------------------------
//  2. ОЙЫН ЛОГИКАСЫ (OpenGL-ге тәуелсіз, сондықтан оңай тексеріледі)
// ---------------------------------------------------------------------
struct Brick    { vec2 pos, half; int hp, maxHp, row; };
struct PowerUp  { vec2 pos; int type; };                       // 0 — кеңейту, 1 — өмір
struct Particle { vec2 pos, vel; float life, maxLife, size; vec3 col; };

const vec3 ROW_COLORS[ROWS] = { {1, .3f, .35f}, {1, .6f, .2f}, {1, .9f, .3f}, {.4f, .9f, .4f}, {.3f, .7f, 1}, {.7f, .45f, 1} };

struct World {
    std::mt19937 rng{ std::random_device{}() };
    std::vector<Brick> bricks;
    std::vector<PowerUp> pups;
    std::vector<Particle> parts;
    float px = 0, pvx = 0, pw = 0.16f, wideTimer = 0, speed = 1.15f, trailTimer = 0, shake = 0, time = 0;
    vec2 ball{ 0, PADDLE_Y + PH + BR }, vel{ 0, 0 };
    int score = 0, lives = 3, level = 1, bricksLeft = 0;
    bool launched = false, over = false;

    World() { buildLevel(); resetBall(); }

    float rnd(float a, float b) { return std::uniform_real_distribution<float>(a, b)(rng); }

    void buildLevel() {
        bricks.clear();
        pups.clear();
        float w = (2 * FW - 0.1f) / COLS;
        for (int r = 0; r < ROWS; r++)
            for (int c = 0; c < COLS; c++) {
                if (level % 2 == 0 && (r + c) % 4 == 3) continue;          // жұп деңгейлерде саңылаулар
                int hp = 1 + (r < std::min(level, 3) ? 1 : 0);             // жоғарғы қатарлар мықтырақ
                bricks.push_back({ vec2(-FW + 0.05f + w * (c + 0.5f), 0.74f - r * 0.095f), vec2(w * 0.46f, 0.036f), hp, hp, r });
            }
        bricksLeft = (int)bricks.size();
    }

    void resetBall() {                       // доп тақтайшаға жабысады, Space басқанда ұшады
        launched = false;
        speed = 1.15f + 0.1f * (level - 1);
        vel = vec2(0);
    }

    void burst(vec2 p, vec3 col, int n, float power) {
        for (int i = 0; i < n; i++) {
            float a = rnd(0, 2 * PI), s = rnd(0.1f, 1.0f) * power, life = rnd(0.35f, 0.8f);
            parts.push_back({ p, vec2(std::cos(a), std::sin(a)) * s, life, life, rnd(0.015f, 0.04f), col });
        }
    }

    void hitBrick(Brick& b) {
        b.hp--;
        speed = std::min(speed * 1.012f, 2.2f);                            // доп бірте-бірте тездейді
        shake = std::max(shake, 0.15f);
        if (b.hp > 0) { burst(b.pos, vec3(1), 4, 0.5f); return; }
        score += 10 * b.maxHp;
        bricksLeft--;
        burst(b.pos, ROW_COLORS[b.row], 14, 0.9f);
        if (rnd(0, 1) < 0.15f) pups.push_back({ b.pos, rnd(0, 1) < 0.7f ? 0 : 1 });   // бонус түсіру
    }

    void loseLife() {
        lives--;
        shake = 0.7f;
        wideTimer = 0;
        burst(vec2(ball.x, PADDLE_Y), vec3(1, .6f, .3f), 30, 1.2f);
        if (lives <= 0) over = true; else resetBall();
    }

    // Доптың бір кішкентай қадамы (тез қозғалғанда қабырғадан өтіп кетпеу үшін бөлшектеп жүреміз)
    void stepBall(float h) {
        ball += vel * h;
        if (ball.x < -FW + BR) { ball.x = -FW + BR; vel.x = std::fabs(vel.x); }          // қабырғалар
        if (ball.x >  FW - BR) { ball.x =  FW - BR; vel.x = -std::fabs(vel.x); }
        if (ball.y > TOP - BR) { ball.y = TOP - BR; vel.y = -std::fabs(vel.y); }

        // Тақтайша: тиген жеріне қарай бұрыш өзгереді (ортасы — тік, шеті — қиғаш)
        if (vel.y < 0 && ball.y > PADDLE_Y - PH * 0.5f && ball.y - BR < PADDLE_Y + PH && std::fabs(ball.x - px) < pw + BR) {
            float a = glm::clamp((ball.x - px) / pw, -1.f, 1.f) * 1.05f;
            vel = vec2(std::sin(a), std::cos(a)) * speed;
            ball.y = PADDLE_Y + PH + BR;
        }

        // Кірпіштер: шеңбер ↔ тіктөртбұрыш. Нормаль бойынша шағылысады, сондықтан бұрыштар да дұрыс
        for (auto& b : bricks) {
            if (b.hp <= 0) continue;
            vec2 c = glm::clamp(ball, b.pos - b.half, b.pos + b.half), d = ball - c;
            float d2 = glm::dot(d, d);
            if (d2 >= BR * BR) continue;
            vec2 n = d2 > 1e-8f ? d / std::sqrt(d2) : vec2(0, ball.y > b.pos.y ? 1.f : -1.f);
            if (glm::dot(vel, n) < 0) vel -= 2.f * glm::dot(vel, n) * n;
            ball += n * (BR - std::sqrt(d2));
            hitBrick(b);
            break;                                                         // бір қадамда бір кірпіш
        }

        if (ball.y < -1.06f) loseLife();

        // Жылдамдық тұрақты болсын, және доп тым көлденең ұшпасын
        float len = glm::length(vel);
        if (len > 1e-4f) vel *= speed / len;
        if (launched && std::fabs(vel.y) < 0.25f * speed) {
            vel.y = (vel.y >= 0 ? 1.f : -1.f) * 0.25f * speed;
            vel.x = (vel.x >= 0 ? 1.f : -1.f) * std::sqrt(speed * speed - vel.y * vel.y);
        }
    }

    void update(float dt, float dir, bool launch) {
        time += dt;
        shake = std::max(0.f, shake - 2.2f * dt);
        for (auto& p : parts) { p.pos += p.vel * dt; p.vel *= (1 - 2 * dt); p.life -= dt; }
        parts.erase(std::remove_if(parts.begin(), parts.end(), [](const Particle& p) { return p.life <= 0; }), parts.end());
        if (over) return;

        // Тақтайша: жылдамдығы біртіндеп өзгереді (инерция); бонус кезінде кеңейеді
        pvx += (dir * 1.9f - pvx) * std::min(1.f, 12 * dt);
        wideTimer = std::max(0.f, wideTimer - dt);
        pw += ((wideTimer > 0 ? 0.26f : 0.16f) - pw) * std::min(1.f, 10 * dt);
        px = glm::clamp(px + pvx * dt, -FW + pw, FW - pw);

        if (!launched) {
            ball = vec2(px, PADDLE_Y + PH + BR + 0.002f);
            if (launch) { launched = true; vel = glm::normalize(vec2(rnd(-0.35f, 0.35f), 1)) * speed; }
        } else {
            int n = (int)std::ceil(glm::length(vel) * dt / (BR * 0.5f));   // қадам саны
            n = std::max(1, n);
            for (int i = 0; i < n && !over && launched; i++) stepBall(dt / n);
            trailTimer -= dt;
            if (trailTimer <= 0) {                                         // доптың артындағы із
                trailTimer = 0.018f;
                parts.push_back({ ball, vec2(0), 0.25f, 0.25f, BR * 0.9f, vec3(.5f, .8f, 1) });
            }
        }

        // Бонустар түсіп келеді, тақтайшаға тисе — қабылданады
        for (auto& u : pups) {
            u.pos.y -= 0.45f * dt;
            if (std::fabs(u.pos.x - px) < pw + 0.03f && std::fabs(u.pos.y - PADDLE_Y) < PH + 0.04f) {
                if (u.type == 0) wideTimer = 8.f; else lives = std::min(lives + 1, 5);
                burst(u.pos, u.type ? vec3(1, .4f, .6f) : vec3(.3f, 1, .5f), 12, 0.7f);
                u.pos.y = -9;
            }
        }
        pups.erase(std::remove_if(pups.begin(), pups.end(), [](const PowerUp& u) { return u.pos.y < -1.1f; }), pups.end());

        if (bricksLeft == 0) { level++; buildLevel(); resetBall(); }       // келесі деңгей
    }
};

// ---------------------------------------------------------------------
//  3. MAIN: енгізу → жаңарту → сызу
// ---------------------------------------------------------------------
int main() {
    if (!glfwInit()) { std::cerr << "GLFW iske qosylmady\n"; return -1; }
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
#ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GLFW_TRUE);
#endif
    GLFWwindow* window = glfwCreateWindow(WIDTH, HEIGHT, "Breakout", nullptr, nullptr);
    if (!window) { std::cerr << "Tereze zhasalmady\n"; glfwTerminate(); return -1; }
    glfwMakeContextCurrent(window);
    glfwSetFramebufferSizeCallback(window, onResize);
    glfwSwapInterval(1);
    if (gladLoadGL(glfwGetProcAddress) == 0) { std::cerr << "GLAD zhuktelmedi\n"; return -1; }
    std::cout << "OpenGL: " << glGetString(GL_VERSION) << "\nGPU:    " << glGetString(GL_RENDERER) << "\n";
    glEnable(GL_BLEND);

    // Геометрияны бір рет жасаймыз
    Mesh meshQuad, meshBrick, meshBg, meshBall, meshGlow;
    { Builder b; b.quad(vec4(1), vec4(1));                                      meshQuad  = b.upload(); }
    { Builder b; b.quad(vec4(1), vec4(.62f, .62f, .72f, 1));                    meshBrick = b.upload(); }
    { Builder b; b.quad(vec4(.02f, .03f, .12f, 1), vec4(.14f, .05f, .26f, 1));  meshBg    = b.upload(); }
    { Builder b; b.disc(24, vec4(1), vec4(.45f, .7f, 1, 1));                    meshBall  = b.upload(); }
    { Builder b; b.disc(24, vec4(1, 1, 1, 1), vec4(1, 1, 1, 0));                meshGlow  = b.upload(); }

    unsigned int shader = makeShader();
    int locModel = glGetUniformLocation(shader, "uModel"), locProj = glGetUniformLocation(shader, "uProj");
    int locTint = glGetUniformLocation(shader, "uTint");

    auto draw = [&](const Mesh& m, vec2 pos, vec2 size, vec4 tint) {
        glm::mat4 model = glm::scale(glm::translate(glm::mat4(1), glm::vec3(pos, 0)), glm::vec3(size, 1));
        glUniformMatrix4fv(locModel, 1, GL_FALSE, glm::value_ptr(model));
        glUniform4f(locTint, tint.r, tint.g, tint.b, tint.a);
        glBindVertexArray(m.vao);
        glDrawElements(GL_TRIANGLES, m.count, GL_UNSIGNED_INT, 0);
    };
    auto normalBlend = [] { glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA); };
    auto glowBlend   = [] { glBlendFunc(GL_SRC_ALPHA, GL_ONE); };              // жарық қосылады

    // Сан жазу: әр жанатын пиксель — кішкентай төртбұрыш
    auto drawNumber = [&](int n, float x, float y, float p, vec4 col, bool right) {
        std::string s = std::to_string(n);
        float x0 = right ? x - (s.size() * 6 - 1) * p : x;
        for (size_t i = 0; i < s.size(); i++)
            for (int r = 0; r < 7; r++)
                for (int c = 0; c < 5; c++)
                    if (DIGITS[s[i] - '0'][r][c] == '#')
                        draw(meshQuad, vec2(x0 + (i * 6 + c + .5f) * p, y - (r + .5f) * p), vec2(p * .92f), col);
    };

    std::mt19937 starRng(7);
    std::vector<vec3> stars;                                                   // x, y, фаза
    for (int i = 0; i < 80; i++) {
        std::uniform_real_distribution<float> d(-1, 1);
        stars.push_back(vec3(d(starRng) * 2.6f, d(starRng), d(starRng) * 3.f));
    }

    World world;
    bool rPrev = false;
    std::string lastTitle;
    float last = (float)glfwGetTime();

    while (!glfwWindowShouldClose(window)) {
        float now = (float)glfwGetTime(), dt = std::min(now - last, 0.05f);
        last = now;

        // --- енгізу ---
        if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS) glfwSetWindowShouldClose(window, true);
        float dir = 0;
        if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS || glfwGetKey(window, GLFW_KEY_LEFT) == GLFW_PRESS)  dir -= 1;
        if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS || glfwGetKey(window, GLFW_KEY_RIGHT) == GLFW_PRESS) dir += 1;
        bool r = glfwGetKey(window, GLFW_KEY_R) == GLFW_PRESS;
        if (r && !rPrev) world = World();                                      // қайта бастау
        rPrev = r;

        // --- жаңарту ---
        world.update(dt, dir, glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS);

        std::string title = "BREAKOUT   Score: " + std::to_string(world.score) + "   Level: " + std::to_string(world.level);
        if (world.over) title += "   GAME OVER - R basyp qaita bastau";
        else if (!world.launched) title += "   Space - ushyru";
        if (title != lastTitle) { glfwSetWindowTitle(window, title.c_str()); lastTitle = title; }

        // --- сызу ---
        glClear(GL_COLOR_BUFFER_BIT);
        glUseProgram(shader);
        float H = std::max(1.f, (FW + 0.1f) / aspect);                         // майдан әрдайым экранға сыяды
        float ox = world.shake * 0.02f * std::sin(now * 90), oy = world.shake * 0.02f * std::cos(now * 77);
        glm::mat4 proj = glm::ortho(-aspect * H + ox, aspect * H + ox, -H + oy, H + oy);
        glUniformMatrix4fv(locProj, 1, GL_FALSE, glm::value_ptr(proj));

        normalBlend();
        draw(meshBg, vec2(0), vec2(aspect * H * 1.3f, H * 1.3f), vec4(1));
        glowBlend();
        draw(meshGlow, vec2(-0.7f, 0.4f), vec2(1.3f), vec4(.45f, .15f, .75f, .18f));      // тұмандық
        draw(meshGlow, vec2(0.8f, -0.2f), vec2(1.1f), vec4(.1f, .45f, .7f, .15f));
        for (auto& s : stars)
            draw(meshGlow, vec2(s.x, s.y), vec2(0.008f + 0.006f * std::sin(s.z)), vec4(.9f, .95f, 1, .4f + .4f * std::sin(now * 2 + s.z)));
        for (float side : { -1.f, 1.f })                                                    // қабырғалар
            draw(meshQuad, vec2(side * (FW + 0.006f), (TOP - 1.1f) / 2), vec2(0.012f, TOP + 1.1f), vec4(.4f, .7f, 1, .5f));
        draw(meshQuad, vec2(0, TOP + 0.006f), vec2(2 * FW + 0.024f, 0.012f), vec4(.4f, .7f, 1, .5f));

        normalBlend();
        for (auto& b : world.bricks) {                                                      // кірпіштер
            if (b.hp <= 0) continue;
            vec3 c = ROW_COLORS[b.row];
            if (b.maxHp > 1) {                                                              // мықты кірпіш: күміс жиекті
                draw(meshBrick, b.pos, b.half * 2.f, vec4(.85f, .87f, .95f, 1));
                draw(meshBrick, b.pos, b.half * 2.f * 0.78f, vec4(c * (b.hp > 1 ? 0.7f : 1.f), 1));
            } else {
                draw(meshBrick, b.pos, b.half * 2.f, vec4(c, 1));
            }
        }
        for (auto& u : world.pups) {                                                        // бонустар
            vec3 c = u.type ? vec3(1, .4f, .6f) : vec3(.3f, 1, .5f);
            glowBlend();
            draw(meshGlow, u.pos, vec2(0.09f), vec4(c, .5f));
            normalBlend();
            draw(meshBall, u.pos, vec2(0.032f), vec4(c, 1));
            draw(meshBall, u.pos, vec2(0.013f), vec4(1));
        }

        // Тақтайша: ортасы тіктөртбұрыш, екі шеті дөңгеленген
        glowBlend();
        draw(meshGlow, vec2(world.px, PADDLE_Y), vec2(world.pw * 1.3f, 0.09f), vec4(.3f, .7f, 1, .4f));
        normalBlend();
        vec4 pc = world.wideTimer > 0 ? vec4(.6f, 1, .7f, 1) : vec4(.75f, .9f, 1, 1);
        draw(meshBrick, vec2(world.px, PADDLE_Y), vec2(world.pw * 2, PH * 2), pc);
        for (float side : { -1.f, 1.f }) draw(meshBall, vec2(world.px + side * world.pw, PADDLE_Y), vec2(PH), pc);

        glowBlend();                                                                        // бөлшектер мен доп ізі
        for (auto& p : world.parts) {
            float t = p.life / p.maxLife;
            draw(meshGlow, p.pos, vec2(p.size * (0.4f + t)), vec4(p.col, t));
        }
        if (!world.over) {
            draw(meshGlow, world.ball, vec2(BR * 3.2f), vec4(.4f, .7f, 1, .55f));
            normalBlend();
            draw(meshBall, world.ball, vec2(BR), vec4(1));
        }

        // HUD: жан (шарлар), ұпай, деңгей
        normalBlend();
        for (int i = 0; i < 5; i++)
            draw(meshBall, vec2(-FW + 0.06f + i * 0.08f, 0.93f), vec2(0.026f), i < world.lives ? vec4(1) : vec4(.3f, .3f, .4f, .5f));
        drawNumber(world.score, FW, 0.97f, 0.009f, vec4(1), true);
        drawNumber(world.level, 0.0f, 0.97f, 0.009f, vec4(.6f, .8f, 1, .9f), false);
        if (world.over) draw(meshQuad, vec2(0), vec2(aspect * H * 2.6f, H * 2.6f), vec4(0, 0, .05f, .6f));

        glfwSwapBuffers(window);
        glfwPollEvents();
    }
    glfwTerminate();
    return 0;
}