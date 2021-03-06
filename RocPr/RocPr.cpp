#include "stdafx.h"
#include <utility>
#include <chrono>
#include <string>
#include <thread>
#include <mutex>
#include <array>

struct Normal {
    double mu;
    double sd;
};

const int kRange = 10;  // Possible values from 0 to kRange

// Distributions for frequencies
constexpr Normal kPositives{3, 1};
constexpr Normal kNegatives{7, 2};

// Graphics settings
// Frequences box
const int kFreqPosX = 10;
const int kFreqPosY = 400;
const int kFreqWidth = 300;
const int kFreqHeight = 300;

const int kConfusionPosX = 400;
const int kConfusionPosY = 450;
const int kConfusionWidth = 250;
const int kConfusionHeight = 100;

const int kRocPosX = 10;
const int kRocPosY = 0;
const int kRocWidth = 300;
const int kRocHeight = 300;

const int kPrPosX = 400;
const int kPrPosY = 0;
const int kPrWidth = 300;
const int kPrHeight = 300;

std::mutex pause;

constexpr int AreaCoefficient() {
    return kFreqWidth / kRange * kFreqHeight;
}

void DrawString(float x, float y, const char* text) {
    glPushMatrix();
    glTranslatef(x, y, 0);
    glScalef(.15f, .15f, .15f);
    for (const char* p = text; *p; p++) {
        glutStrokeCharacter(GLUT_STROKE_ROMAN, *p);
    }
    glPopMatrix();
}

double CalculateNormal(double sd, double x) {
    constexpr double sqrtPi = 1.7724538f;
    constexpr double factor = 1.f / (M_SQRT2 * sqrtPi);
    const double res = factor / sd * std::exp(-x * x / (2 * sd * sd));
    return res;
}

double CalculateNormalCFD(double mu, double sd, double x) {
    return 0.5 * std::erfc((mu - x) / sd * M_SQRT1_2);
}

void DrawNormal(float sd, float leftTailLen, float rightTailLen, int stepsCount) {
    glBegin(GL_LINE_STRIP);
    const float stepSize = (leftTailLen + rightTailLen) / stepsCount;
    for (float x = -leftTailLen; x < rightTailLen; x += stepSize) {
        const float y = CalculateNormal(sd, x);
        glVertex3f(x, y, 0);
    }
    glEnd();
}

void DrawConfusionMatrix(int fp, int fn, int tp, int tn) {
    const int indent = 10;
    glColor4f(0, 1, 0, 0.5);
    DrawString(indent, indent, ("FP=" + std::to_string(fp)).c_str());
    DrawString(indent, kConfusionHeight / 2 + indent, ("TN=" + std::to_string(tn)).c_str());
    glColor4f(1, 0, 0, 0.5);
    DrawString(kConfusionWidth / 2 + indent, kConfusionHeight / 2 + indent, ("TP=" + std::to_string(tp)).c_str());
    DrawString(kConfusionWidth / 2 + indent, indent, ("FN=" + std::to_string(fn)).c_str());
    glColor4f(.9f, .9f, .9f, 0.5f);
    glBegin(GL_LINE_LOOP);
    glVertex3f(0, 0, 0);
    glVertex3f(kConfusionWidth, 0, 0);
    glVertex3f(kConfusionWidth, kConfusionHeight, 0);
    glVertex3f(0, kConfusionHeight, 0);
    glEnd();
    glBegin(GL_LINES);
    glVertex3f(kConfusionWidth / 2, 0, 0);
    glVertex3f(kConfusionWidth / 2, kConfusionHeight, 0);
    glVertex3f(0, kConfusionHeight / 2, 0);
    glVertex3f(kConfusionWidth, kConfusionHeight / 2, 0);
    glEnd();
}

static void Reshape(int width, int height)
{
    glViewport(0, 0, width, height);

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
}

// Data to be rendered
int gFp, gFn, gTp, gTn;
bool gShowConfusionMatrix;
bool gShowCuttoff;
bool gShowPr;
int gCutoffOffset;
std::array<std::pair<int, int>, kFreqWidth> gRoc;
std::array<std::pair<float, float>, kFreqWidth> gPr;

void CalculatePr(int i) {
    const int tp = gRoc[i].first;
    const int fp = gRoc[i].second;
    const int fn = 1 * AreaCoefficient() - tp;
    const float precision = static_cast<float>(tp) / (tp + fp);
    const float recall = static_cast<float>(tp) / (tp + fn);
    gPr[i] = { recall, precision };
}

static void Draw(void)
{
    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    glOrtho(0.0, glutGet(GLUT_WINDOW_WIDTH), 0.0, glutGet(GLUT_WINDOW_HEIGHT), -1.0, 1.0);
    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();

    glClear(GL_COLOR_BUFFER_BIT);
    glColor4f(1, 1, 1, 0.5);

    {
        glPushMatrix();
        glTranslatef(kFreqPosX, kFreqPosY, 0);

        {
            glColor4f(1, 0, 0, 0.5);
            glPushMatrix();
            glTranslatef(kFreqWidth / kRange * kPositives.mu, 0, 0);
            glScalef(kFreqWidth / kRange, kFreqHeight, 1.f);
            DrawNormal(kPositives.sd, kPositives.mu, kRange - kPositives.mu, kFreqWidth);
            glPopMatrix();

            glPushMatrix();
            DrawString(kFreqWidth / 2, kFreqHeight + 25, "positive");
            glPopMatrix();
        }

        {
            glColor4f(0, 1, 0, 0.5);
            glPushMatrix();
            glTranslatef(kFreqWidth / kRange * kNegatives.mu, 0, 0);
            glScalef(kFreqWidth / kRange, kFreqHeight, 1.f);
            DrawNormal(kNegatives.sd, kNegatives.mu, kRange - kNegatives.mu, kFreqWidth);
            glPopMatrix();

            glPushMatrix();
            DrawString(kFreqWidth / 2, kFreqHeight + 50, "negative");
            glPopMatrix();
        }

        glPopMatrix();
    }

    if (gShowConfusionMatrix) {
        glPushMatrix();
        glTranslatef(kConfusionPosX, kConfusionPosY, 0);
        DrawConfusionMatrix(gFp, gFn, gTp, gTn);
        glPopMatrix();
    }

    if (gShowCuttoff) {
        glPushMatrix();
        glTranslatef(kFreqPosX, kFreqPosY, 0);
        glColor4f(.0, .0, .0, 0.5);
        glBegin(GL_LINES);
        glVertex3f(gCutoffOffset, 0, 0);
        glVertex3f(gCutoffOffset, kFreqHeight, 0);
        glEnd();
        glPopMatrix();
    }

    {
        glPushMatrix();
        glTranslatef(kRocPosX, kRocPosY, 0);
        glColor4f(1., 1., 1., 0.5);
        DrawString(kRocWidth / 2, kRocHeight + 25, "ROC");
        glBegin(GL_POINTS);
        for (int i = 0; i < gCutoffOffset; ++i) {
            glVertex3f(static_cast<float>(gRoc[i].first) / AreaCoefficient() * kRocWidth, static_cast<float>(gRoc[i].second) / AreaCoefficient() * kRocHeight, 0);
        }
        glEnd();
        glPopMatrix();
    }

    if (gShowPr) {
        glPushMatrix();
        glTranslatef(kPrPosX, kPrPosY, 0);
        glColor4f(1., 1., 1., 0.5);
        DrawString(kPrWidth / 2, kPrHeight + 25, "PR");
        glBegin(GL_POINTS);
        for (int i = 0; i < gCutoffOffset; ++i) {
            glVertex3f(gPr[i].first * kPrWidth, gPr[i].second * kPrHeight, 0);
        }
        glEnd();
        glPopMatrix();
    }

    glutSwapBuffers();

    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glMatrixMode(GL_MODELVIEW);
    glPopMatrix();
}

void Idle() {
    glutPostRedisplay();
}

void Keyboard(unsigned char key, int, int) {
    if (key == 32) {
        if (!pause.try_lock()) {
            pause.unlock();
        }
    }
}

class Logic {
public:
    enum class State {
        BUILD_ROC,
        CALCULATE_PR,
        DONE
    };

    void Step() {
        switch (_state) {
        case State::BUILD_ROC:
            if (_rocProgress < kFreqWidth) {
                gShowConfusionMatrix = gShowCuttoff = true;
                gShowPr = false;
                gCutoffOffset = _rocProgress;
                BuildConfusionMatrix(kPositives, kNegatives, gFp, gFn, gTp, gTn);
                gRoc[_rocProgress] = { gFp, gTp };
                ++_rocProgress;
            }
            else {
                _state = State::CALCULATE_PR;
            }
            break;
        case State::CALCULATE_PR:
            if (_prProgress < kFreqWidth) {
                gShowConfusionMatrix = gShowCuttoff = false;
                gShowPr = true;
                gCutoffOffset = _prProgress;
                CalculatePr(_prProgress++);
            }
            else {
                _state = State::DONE;
            }
            break;
        case State::DONE:
            break;
        default:
            throw;
        }
    }

    State GetState() { return _state; }

private:
    void BuildConfusionMatrix(Normal positive, Normal negative, int& fp, int& fn, int& tp, int& tn) {
        double x = _rocProgress * static_cast<double>(kRange) / kFreqWidth;
        fn = CalculateNormalCFD(positive.mu, positive.sd, x) * AreaCoefficient() + .5;
        tp = 1 * AreaCoefficient() - fn;
        tn = CalculateNormalCFD(negative.mu, negative.sd, x) * AreaCoefficient() + .5;
        fp = 1 * AreaCoefficient() - fp;
        if (positive.mu < negative.mu) {
            std::swap(fn, tp);
            std::swap(fp, tn);
        }
    }

private:
    State _state = State::BUILD_ROC;
    int _rocProgress = 0;
    int _prProgress = 0;
};

class Launcher {
public:
    Launcher(std::chrono::milliseconds::rep dt) : _dt(dt) {}

    void Run() {
        Logic logic;
        while (logic.GetState() != Logic::State::DONE) {
            while (!pause.try_lock());
            pause.unlock();
            _startTime = Millis();
            logic.Step();
            WaitForDeltaTime();
        }
    }

private:
    std::chrono::milliseconds::rep Millis() {
        auto now = std::chrono::system_clock::now();
        auto duration = now.time_since_epoch();
        return std::chrono::duration_cast<std::chrono::milliseconds>(duration).count();
    }

    void WaitForDeltaTime() {
        while (Millis() - _startTime < _dt);
    }

    std::chrono::milliseconds::rep _startTime;
    const std::chrono::milliseconds::rep _dt;    
};

void Run() {
    Launcher l(10);
    l.Run();
}

int main(int argc, char **argv)
{
    GLenum type;

    glutInit(&argc, argv);

    type = GLUT_RGB | GLUT_ACCUM | GLUT_DOUBLE;
    glutInitDisplayMode(type);
    glutInitWindowSize(700, 800);
    glutCreateWindow("RocPr");

    glClearColor(.2f, .2f, .2f, .5f);

    std::thread th(Run);

    glutReshapeFunc(Reshape);
    glutDisplayFunc(Draw);
    glutIdleFunc(Idle);
    glutKeyboardFunc(Keyboard);
    glutMainLoop();

    th.join();
    return 0;
}

