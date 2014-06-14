#include "ViewerWidget.hpp"
#include <render/Model.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <QMouseEvent>
#include <engine/GameObject.hpp>
#include <engine/Animator.hpp>
#include <QFileDialog>
#include <algorithm>
#include <objects/InstanceObject.hpp>

ViewerWidget::ViewerWidget(QWidget* parent, const QGLWidget* shareWidget, Qt::WindowFlags f)
: QGLWidget(parent, shareWidget, f), gworld(nullptr), dummyObject(nullptr), currentObjectID(0),
  _lastModel(nullptr), canimation(nullptr), viewDistance(1.f), dragging(false),
  _frameWidgetDraw(nullptr), _frameWidgetGeom(nullptr)
{
}

struct WidgetVertex {
	float x, y, z;
	static const AttributeList vertex_attributes() {
		return {
			{ATRS_Position, 2, sizeof(WidgetVertex),  0ul}
		};
	}
};

std::vector<WidgetVertex> widgetVerts = {
	{-1.f, 0.f, 0.f},
	{ 1.f, 0.f, 0.f},
	{ 0.f,-1.f, 0.f},
	{ 0.f, 1.f, 0.f},
	{ 0.f, 0.f,-1.f},
	{ 0.f, 0.f, 1.f}
};

void ViewerWidget::initializeGL()
{
	QGLWidget::initializeGL();
	timer.setInterval(16);
	connect(&timer, SIGNAL(timeout()), SLOT(updateGL()));
	timer.start();
	
	glewExperimental = 1;
	glewInit();

	_frameWidgetDraw = new DrawBuffer;
	_frameWidgetDraw->setFaceType(GL_LINES);
	_frameWidgetGeom = new GeometryBuffer;
	_frameWidgetGeom->uploadVertices(widgetVerts);
	_frameWidgetDraw->addGeometry(_frameWidgetGeom);
}

void ViewerWidget::resizeGL(int w, int h)
{
	QGLWidget::resizeGL(w, h);
	glViewport(0, 0, w, h);
}

void ViewerWidget::paintGL()
{
	glClearColor(0.3f, 0.3f, 0.3f, 1.f);
	glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
	
	glViewport(0, 0, width(), height());
	
	if( gworld == nullptr ) return;

	auto& r = gworld->renderer;
	
	r.camera.frustum.far = 500.f;
	r.camera.frustum.near = 0.1f;
	r.camera.frustum.fov = 90.f;
	r.camera.frustum.aspectRatio = width()/(height()*1.f);

	if(dummyObject && dummyObject->animator) {
		dummyObject->animator->tick(1.f/60.f);
	}
	
	if(dummyObject) {
		gworld->_work->update();

		if( dummyObject->model->model != _lastModel ) {
			_lastModel = dummyObject->model->model;
			emit modelChanged(_lastModel);
		}


		glEnable(GL_DEPTH_TEST);
		
		glm::mat4 m;
		
		glUseProgram(r.worldProgram);

		glUniform1f(r.uniFogStart, 90.f);
		glUniform1f(r.uniFogEnd, 100.f);

		glUniform4f(r.uniAmbientCol, .1f, .1f, .1f, 1.f);
		glUniform4f(r.uniDynamicCol, 1.f, 1.f, 1.f, 1.f);
		//glUniform3f(uniSunDirection, sunDirection.x, sunDirection.y, sunDirection.z);
		glUniform1f(r.uniMatDiffuse, 0.9f);
		glUniform1f(r.uniMatAmbient, 0.1f);
		
		glm::mat4 proj = r.camera.frustum.projection();
		glm::vec3 eye(sin(viewAngles.x) * cos(viewAngles.y), cos(viewAngles.x) * cos(viewAngles.y), sin(viewAngles.y));
		glm::mat4 view = glm::lookAt(eye * viewDistance, glm::vec3(0.f, 0.f, 0.f), glm::vec3(0.f, 0.f, 1.f));
		glUniformMatrix4fv(r.uniView, 1, GL_FALSE, glm::value_ptr(view));
		glUniformMatrix4fv(r.uniProj, 1, GL_FALSE, glm::value_ptr(proj));
		
		if( dummyObject->model->model ) {
			gworld->renderer.renderModel(dummyObject->model->model, m, dummyObject);

			drawFrameWidget(dummyObject->model->model->frames[dummyObject->model->model->rootFrameIdx]);
		}
	}
}

void ViewerWidget::drawFrameWidget(ModelFrame* f, const glm::mat4& m)
{
	auto thisM = m * f->getTransform();
	if(f->getGeometries().size() == 0) {
		glBindTexture(GL_TEXTURE_2D, 0);
		glUniform4f(gworld->renderer.uniCol, 1.f, 1.f, 1.f, 1.f);
		glUniformMatrix4fv(gworld->renderer.uniModel, 1, GL_FALSE, glm::value_ptr(thisM));
		glBindVertexArray(_frameWidgetDraw->getVAOName());
		glDrawArrays(_frameWidgetDraw->getFaceType(), 0, 6);
	}
	for(auto c : f->getChildren()) {
		drawFrameWidget(c, thisM);
	}
}

GameWorld* ViewerWidget::world()
{
	return gworld;
}

void ViewerWidget::showItem(qint16 item)
{
	currentObjectID = item;

	if( dummyObject ) gworld->destroyObject( dummyObject );

	dummyObject = gworld->createInstance(item, {});
}

void ViewerWidget::showAnimation(Animation *anim)
{
	canimation = anim;
	if(dummyObject) {
		if(dummyObject->animator == nullptr) {
			dummyObject->animator = new Animator;
			dummyObject->animator->setModel(dummyObject->model->model);
		}
		dummyObject->animator->setAnimation(canimation);
	}
}

void ViewerWidget::exportModel()
{
	QString toSv = QFileDialog::getSaveFileName(this,
												"Export Model",
												QDir::homePath(),
												"Model (*.DFF)");

	if( toSv.size() == 0 ) return;

	auto it = world()->objectTypes.find(currentObjectID);
	if( it != world()->objectTypes.end() ) {
		for( auto& archive : world()->gameData.archives ) {
			for(size_t i = 0; i < archive.second.getAssetCount(); ++i) {
				auto& assetI = archive.second.getAssetInfoByIndex(i);
				std::string q(assetI.name);
				std::transform(q.begin(), q.end(), q.begin(), ::tolower);
				if( q.find(it->second->modelName) != q.npos ) {
					archive.second.saveAsset(q, toSv.toStdString());
				}
			}
		}
	}
}

Model* ViewerWidget::currentModel() const
{
	return _lastModel;
}

void ViewerWidget::setGamePath(const std::string &path)
{
	if( gworld ) delete gworld;
	gworld = new GameWorld(path);

	gworld->gameData.loadIMG("/models/gta3");
	gworld->gameData.loadIMG("/models/txd");

	gworld->load();
	for(auto it = gworld->gameData.ideLocations.begin();
		it != gworld->gameData.ideLocations.end();
		++it) {
		gworld->defineItems(it->second);
	}

	emit dataLoaded(gworld);
}

void ViewerWidget::mousePressEvent(QMouseEvent* e)
{
	dragging = true;
	dstart = e->localPos();
	dastart = viewAngles;
}

void ViewerWidget::mouseReleaseEvent(QMouseEvent*)
{
	dragging = false;
}

void ViewerWidget::mouseMoveEvent(QMouseEvent* e)
{
    if(dragging) {
		auto d = e->localPos() - dstart;
		viewAngles = dastart + glm::vec2(d.x(), d.y()) * 0.01f;
	}
}

void ViewerWidget::wheelEvent(QWheelEvent* e)
{
	viewDistance = qMax(viewDistance - e->angleDelta().y() / 240.f, 0.5f);
}
