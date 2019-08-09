#ifndef LOADNONDESAMPLE_H_
#define LOADNONDESAMPLE_H_

#include "gameplay.h"
#include "Sample.h"

using namespace gameplay;

/**
 * Samples programattically contructing a scene.
 */
class LoadNodeSample : public Sample, Control::Listener
{
public:

    LoadNodeSample();

    void touchEvent(Touch::TouchEvent evt, int x, int y, unsigned int contactIndex);

	void keyEvent(Keyboard::KeyEvent evt, int key);

	//bool mouseEvent(Mouse::MouseEvent evt, int x, int y, int wheelDelta);

	void controlEvent(Control* control, EventType evt);
protected:

    void initialize();

	bool initializeMaterials(Node* node);

    void finalize();

    void update(float elapsedTime);

    void render(float elapsedTime);

private:
	enum CameraMovement
	{
		MOVE_FORWARD = (1 << 0),
		MOVE_BACKWARD = (1 << 1),
		MOVE_LEFT = (1 << 2),
		MOVE_RIGHT = (1 << 3),
		MOVE_UP = (1 << 4),
		MOVE_DOWN = (1 << 5)
	};
    bool drawScene(Node* node);
	unsigned _inputMask;
    Font* _font;
    Scene* _scene;
    Node* _armature;
	Node* _cameraNode;
	Vector3 _cameraAcceleration;
	Form* _formSelect;
};

#endif
