#include "LoadNodeSample.h"
#include "SamplesGame.h"

#if defined(ADD_SAMPLE)
    ADD_SAMPLE("Graphics", "Load Node", LoadNodeSample, 17);
#endif

static const float MOUSE_SPEED = 0.125f;
static const float MASS = 1.8f;
static const float FRICTION = 0.9f;
static const float SPEED = 5.5f;

static Mesh* createCubeMesh(float size = 1.0f)
{
    float a = size * 0.5f;
    float vertices[] =
    {
        -a, -a,  a,    0.0,  0.0,  1.0,   0.0, 0.0,
         a, -a,  a,    0.0,  0.0,  1.0,   1.0, 0.0,
        -a,  a,  a,    0.0,  0.0,  1.0,   0.0, 1.0,
         a,  a,  a,    0.0,  0.0,  1.0,   1.0, 1.0,
        -a,  a,  a,    0.0,  1.0,  0.0,   0.0, 0.0,
         a,  a,  a,    0.0,  1.0,  0.0,   1.0, 0.0,
        -a,  a, -a,    0.0,  1.0,  0.0,   0.0, 1.0,
         a,  a, -a,    0.0,  1.0,  0.0,   1.0, 1.0,
        -a,  a, -a,    0.0,  0.0, -1.0,   0.0, 0.0,
         a,  a, -a,    0.0,  0.0, -1.0,   1.0, 0.0,
        -a, -a, -a,    0.0,  0.0, -1.0,   0.0, 1.0,
         a, -a, -a,    0.0,  0.0, -1.0,   1.0, 1.0,
        -a, -a, -a,    0.0, -1.0,  0.0,   0.0, 0.0,
         a, -a, -a,    0.0, -1.0,  0.0,   1.0, 0.0,
        -a, -a,  a,    0.0, -1.0,  0.0,   0.0, 1.0,
         a, -a,  a,    0.0, -1.0,  0.0,   1.0, 1.0,
         a, -a,  a,    1.0,  0.0,  0.0,   0.0, 0.0,
         a, -a, -a,    1.0,  0.0,  0.0,   1.0, 0.0,
         a,  a,  a,    1.0,  0.0,  0.0,   0.0, 1.0,
         a,  a, -a,    1.0,  0.0,  0.0,   1.0, 1.0,
        -a, -a, -a,   -1.0,  0.0,  0.0,   0.0, 0.0,
        -a, -a,  a,   -1.0,  0.0,  0.0,   1.0, 0.0,
        -a,  a, -a,   -1.0,  0.0,  0.0,   0.0, 1.0,
        -a,  a,  a,   -1.0,  0.0,  0.0,   1.0, 1.0
    };
    short indices[] = 
    {
        0, 1, 2, 2, 1, 3, 4, 5, 6, 6, 5, 7, 8, 9, 10, 10, 9, 11, 12, 13, 14, 14, 13, 15, 16, 17, 18, 18, 17, 19, 20, 21, 22, 22, 21, 23
    };
    unsigned int vertexCount = 24;
    unsigned int indexCount = 36;
    VertexFormat::Element elements[] =
    {
        VertexFormat::Element(VertexFormat::POSITION, 3),
        VertexFormat::Element(VertexFormat::NORMAL, 3),
        VertexFormat::Element(VertexFormat::TEXCOORD0, 2)
    };
    Mesh* mesh = Mesh::createMesh(VertexFormat(elements, 3), vertexCount, false);
    if (mesh == NULL)
    {
        GP_ERROR("Failed to create mesh.");
        return NULL;
    }
    mesh->setVertexData(vertices, 0, vertexCount);
    MeshPart* meshPart = mesh->addPart(Mesh::TRIANGLES, Mesh::INDEX16, indexCount, false);
    meshPart->setIndexData(indices, 0, indexCount);
    return mesh;
}

LoadNodeSample::LoadNodeSample()
    : _font(NULL), _scene(NULL), _armature(NULL),_inputMask(0u), _cameraNode(NULL)
{
}

void LoadNodeSample::initialize()
{
    // Create the font for drawing the framerate.
    _font = Font::create("res/ui/arial.gpb");

    // Create a new empty scene.
    //_scene = Scene::create();
	_scene = Scene::load("res/common/spearman2.gpb");
	// Visit all the nodes in the scene, drawing the models/mesh.
	
	/*
    // Create the camera.
    Camera* camera = Camera::createPerspective(45.0f, getAspectRatio(), 1.0f, 20.0f);
    Node* cameraNode = _scene->addNode("camera");

    // Attach the camera to a node. This determines the position of the camera.
    cameraNode->setCamera(camera);

    // Make this the active camera of the scene.
    _scene->setActiveCamera(camera);
    SAFE_RELEASE(camera);
	
    // Move the camera to look at the origin.
    cameraNode->translate(0, 2, 5);
    cameraNode->rotateX(MATH_DEG_TO_RAD(-11.25f));
	*/

	// Set up a first person style camera
	const Vector3 camStartPosition(0.f, 1.f, -10.f);
	_cameraNode = Node::create("cameraNode");
	_cameraNode->setTranslation(camStartPosition);

	Node* camPitchNode = Node::create();
	Matrix m;
	Matrix::createLookAt(_cameraNode->getTranslation(), Vector3::zero(), Vector3::unitY(), &m);
	camPitchNode->rotate(m);
	_cameraNode->addChild(camPitchNode);
	_scene->addNode(_cameraNode);
	Camera* camera = Camera::createPerspective(45.f, Game::getInstance()->getAspectRatio(), 0.1f, 150.f);
	camPitchNode->setCamera(camera);
	_scene->setActiveCamera(camera);
	SAFE_RELEASE(camera);
	SAFE_RELEASE(camPitchNode);


    // Create a white light.
    Light* light = Light::createDirectional(0.75f, 0.75f, 0.75f);
    Node* lightNode = _scene->addNode("light");
    lightNode->setLight(light);
    // Release the light because the node now holds a reference to it.
    SAFE_RELEASE(light);

	_formSelect = Form::create("animation select", NULL, Layout::LAYOUT_VERTICAL);
	_formSelect->setWidth(300);
	_formSelect->setHeight(1, true);
	_formSelect->setScroll(Container::SCROLL_VERTICAL);

	//_formSelect->setPosition(50, 50);
	//_formSelect->setFont(_font);
	//_formSelect->setFontSize(24);
	

	// Load shapes
	//Bundle* bundle = Bundle::create("res/common/spearman2.gpb");
	//_armature = bundle->loadNode("worker_stand_Nonetext");
	//SAFE_RELEASE(bundle);
	//_scene->addNode(_armature);

	//_armature->scale(0.1,0.1,0.1);

	//_scene->visit(this, &LoadNodeSample::initializeMaterials);
	/*
	Model* model = dynamic_cast<Model*>(_armature->getDrawable());
    // Create the material for the cube model and assign it to the first mesh part.
    Material* material = model->setMaterial("res/shaders/textured.vert", "res/shaders/textured.frag", "DIRECTIONAL_LIGHT_COUNT 1");

    // These parameters are normally set in a .material file but this example sets them programmatically.
    // Bind the uniform "u_worldViewProjectionMatrix" to use the WORLD_VIEW_PROJECTION_MATRIX from the scene's active camera and the node that the model belongs to.
    material->setParameterAutoBinding("u_worldViewProjectionMatrix", "WORLD_VIEW_PROJECTION_MATRIX");
    material->setParameterAutoBinding("u_inverseTransposeWorldViewMatrix", "INVERSE_TRANSPOSE_WORLD_VIEW_MATRIX");
    // Set the ambient color of the material.
    material->getParameter("u_ambientColor")->setValue(Vector3(0.2f, 0.2f, 0.2f));

    // Bind the light's color and direction to the material.
    material->getParameter("u_directionalLightColor[0]")->setValue(lightNode->getLight()->getColor());
    material->getParameter("u_directionalLightDirection[0]")->bindValue(lightNode, &Node::getForwardVectorWorld);

    // Load the texture from file.
    Texture::Sampler* sampler = material->getParameter("u_diffuseTexture")->setValue("res/png/texture_worker.png", true);
    sampler->setFilterMode(Texture::LINEAR_MIPMAP_LINEAR, Texture::LINEAR);
   
	material->getStateBlock()->setCullFace(true);
    material->getStateBlock()->setDepthTest(true);
    material->getStateBlock()->setDepthWrite(true);
	*/

	//setMouseCaptured(true);
	//setMultiTouch(true);

	//_armature->rotateY(MATH_PIOVER4);
	//_armature->setEnabled(true);


	Label *lbl =  Label::create("btnList");
	lbl->setSize(200, 40);
	lbl->setText("animation list:");
	_formSelect->addControl(lbl);
	lbl->release();

	//_scene->removeAnimation("Armature|attack");

	Animation *anim = _scene->findAnimation("spearman2");
	anim->createClipsFromTakeInfo();

	for (int i = 0; i < anim->getClipCount();i++) {
		AnimationClip *clip = anim->getClip(i);
		if (clip != NULL) {
			Button *btn = Button::create(clip->getId());

			btn->setText(clip->getId());
			btn->setWidth(1, true);
			btn->setHeight(50);
			btn->addListener(this, Control::Listener::CLICK);
			_formSelect->addControl(btn);
			SAFE_RELEASE(btn);
		}

	}
	_formSelect->setFocus();
}

void LoadNodeSample::controlEvent(Control* control, EventType evt)
{
	if (evt == CLICK)
	{
		Animation  *anim = _scene->findAnimation("spearman2");
		anim->play(control->getId());
	}
}

bool LoadNodeSample::initializeMaterials(Node* node)
{
	GP_WARN("initializeMaterials node = '%s'.", node->getId());
	Drawable *d = node->getDrawable();
	if (d)
	{
		Model* model = dynamic_cast<Model*>(d);
		Node* lightNode = _scene->findNode("light");
		Material* material = model->getMaterial();
		
	}
	return true;
}

void LoadNodeSample::finalize()
{
    SAFE_RELEASE(_font);
    SAFE_RELEASE(_scene);
	SAFE_RELEASE(_cameraNode);
	SAFE_RELEASE(_formSelect);
}

void LoadNodeSample::update(float elapsedTime)
{
    // Rotate the directional light.
	//_armature->rotateY(elapsedTime * 0.001 * MATH_PI);

	Vector3 force;
	const float minVal = 0.1f;
	if ((_inputMask & MOVE_FORWARD))
		force += _cameraNode->getFirstChild()->getForwardVectorWorld();
	if (_inputMask & MOVE_BACKWARD )
		force -= _cameraNode->getFirstChild()->getForwardVectorWorld();
	if (_inputMask & MOVE_LEFT )
		force += _cameraNode->getRightVectorWorld();
	if (_inputMask & MOVE_RIGHT)
		force -= _cameraNode->getRightVectorWorld();

	if (_inputMask & MOVE_UP)
		force += _cameraNode->getUpVectorWorld();
	if (_inputMask & MOVE_DOWN)
		force -= _cameraNode->getUpVectorWorld();

	if (force.lengthSquared() > 1.f) force.normalize();

	_cameraAcceleration += force / MASS;
	_cameraAcceleration *= FRICTION;
	if (_cameraAcceleration.lengthSquared() < 0.01f)
		_cameraAcceleration = Vector3::zero();
	_cameraNode->translate(_cameraAcceleration * SPEED * (elapsedTime / 1000.f));

}

void LoadNodeSample::render(float elapsedTime)
{
    // Clear the color and depth buffers
    clear(CLEAR_COLOR_DEPTH, 0.0f, 0.0f, 0.0f, 1.0f, 1.0f, 0);

    // Visit all the nodes in the scene, drawing the models.
    _scene->visit(this, &LoadNodeSample::drawScene);

	Game::getInstance()->getPhysicsController()->drawDebug(_scene->getActiveCamera()->getViewProjectionMatrix());

	_formSelect->draw();

    drawFrameRate(_font, Vector4(0, 0.5f, 1, 1), 5, 1, getFrameRate());
}

void LoadNodeSample::keyEvent(Keyboard::KeyEvent evt, int key)
{
	if (evt == Keyboard::KEY_PRESS)
	{
		switch (key)
		{

		case Keyboard::KEY_W:
		case Keyboard::KEY_UP_ARROW:
			_inputMask |= MOVE_FORWARD;
			break;
		case Keyboard::KEY_S:
		case Keyboard::KEY_DOWN_ARROW:
			_inputMask |= MOVE_BACKWARD;
			break;
		case Keyboard::KEY_A:
		case Keyboard::KEY_LEFT_ARROW:
			_inputMask |= MOVE_LEFT;
			break;
		case Keyboard::KEY_D:
		case Keyboard::KEY_RIGHT_ARROW:
			_inputMask |= MOVE_RIGHT;
			break;
		case Keyboard::KEY_Q:
			_inputMask |= MOVE_UP;
			break;
		case Keyboard::KEY_E:
			_inputMask |= MOVE_DOWN;
			break;
		}
	}
	else if (evt == Keyboard::KEY_RELEASE)
	{
		switch (key)
		{
		case Keyboard::KEY_W:
		case Keyboard::KEY_UP_ARROW:
			_inputMask &= ~MOVE_FORWARD;
			break;
		case Keyboard::KEY_S:
		case Keyboard::KEY_DOWN_ARROW:
			_inputMask &= ~MOVE_BACKWARD;
			break;
		case Keyboard::KEY_A:
		case Keyboard::KEY_LEFT_ARROW:
			_inputMask &= ~MOVE_LEFT;
			break;
		case Keyboard::KEY_D:
		case Keyboard::KEY_RIGHT_ARROW:
			_inputMask &= ~MOVE_RIGHT;
			break;
		case Keyboard::KEY_Q:
			_inputMask &= ~MOVE_UP;
			break;
		case Keyboard::KEY_E:
			_inputMask &= ~MOVE_DOWN;
			break;
		}
	}
}


void LoadNodeSample::touchEvent(Touch::TouchEvent evt, int x, int y, unsigned int contactIndex)
{
    switch (evt)
    {
    case Touch::TOUCH_PRESS:
        if (x < 75 && y < 50)
        {
            // Toggle Vsync if the user touches the top left corner
            setVsync(!isVsync());
        }
        break;
    case Touch::TOUCH_RELEASE:
        break;
    case Touch::TOUCH_MOVE:
        break;
    };
}

/*
bool LoadNodeSample::mouseEvent(Mouse::MouseEvent evt, int x, int y, int wheelDelta)
{
	switch (evt)
	{
	case Mouse::MOUSE_MOVE:
	{
		float xMovement = MATH_DEG_TO_RAD(-x * MOUSE_SPEED);
		float yMovement = MATH_DEG_TO_RAD(-y * MOUSE_SPEED);
		_cameraNode->rotateY(xMovement);
		_cameraNode->getFirstChild()->rotateX(yMovement);

	}
	return true;
	case Mouse::MOUSE_PRESS_LEFT_BUTTON:
		_inputMask |= MOVE_FORWARD;
		return true;
	case Mouse::MOUSE_RELEASE_LEFT_BUTTON:
		_inputMask &= ~MOVE_FORWARD;
		return true;
	case Mouse::MOUSE_PRESS_RIGHT_BUTTON:
		_inputMask |= MOVE_BACKWARD;
		return true;
	case Mouse::MOUSE_RELEASE_RIGHT_BUTTON:
		_inputMask &= ~MOVE_BACKWARD;
		return true;
	default: return false;
	}
	return false;
}
*/

bool LoadNodeSample::drawScene(Node* node)
{
    //Drawable* drawable = node->getDrawable();
    //if (drawable)
    //    drawable->draw();

	Camera* camera = _scene->getActiveCamera();
	Drawable* drawable = node->getDrawable();
	if (dynamic_cast<Model*>(drawable))
	{
		if (!node->getBoundingSphere().intersects(camera->getFrustum()))
			return true;
	}
	if (drawable)
	{
		drawable->draw();
	}

    return true;
}