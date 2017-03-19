//
// This file is part of the WoW++ project.
// 
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License for more details.
// 
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software 
// Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
//
// World of Warcraft, and all World of Warcraft or Warcraft art, images,
// and lore are copyrighted by Blizzard Entertainment, Inc.
// 

#include "pch.h"
#include "transform_widget.h"
#include "selection.h"
#include "selected.h"

namespace wowpp
{
	namespace editor
	{
		// Constant initialization
		const float TransformWidget::LineLength = 1.0f;
		const float TransformWidget::CenterOffset = 0.3f;
		const float TransformWidget::SquareLength = 0.5f;
		const float TransformWidget::AxisBoxWidth = 0.1f;
		const float TransformWidget::PlaneHeight = 0.1f;
		const float TransformWidget::TipLength = 0.3f;
		const float TransformWidget::OrthoScale = 150.0f;
		const Ogre::ColourValue TransformWidget::ColorXAxis = Ogre::ColourValue(1.0f, 0.0f, 0.0f);
		const Ogre::ColourValue TransformWidget::ColorYAxis = Ogre::ColourValue(0.0f, 1.0f, 0.0f);
		const Ogre::ColourValue TransformWidget::ColorZAxis = Ogre::ColourValue(0.0f, 0.0f, 1.0f);
		const Ogre::ColourValue TransformWidget::ColorSelectedAxis = Ogre::ColourValue(1.0f, 1.0f, 0.0f);
		const Ogre::String TransformWidget::AxisPlaneName = "Editor/AxisPlane";
		const Ogre::String TransformWidget::ArrowMeshName = "Arrow.mesh";
		const float TransformWidget::OuterRadius = 1.0f;
		const float TransformWidget::InnerRadius = 0.8f;
		const Ogre::String TransformWidget::CircleMeshName = "Editor/RotationCircle";
		const Ogre::String TransformWidget::FullCircleMeshName = "Editor/FullRotationCircle";
		const Ogre::String TransformWidget::ScaleAxisPlaneName = "Editor/ScaleAxisPlane";
		const Ogre::String TransformWidget::ScaleContentPlaneName = "Editor/ScaleContentPlane";

		TransformWidget::TransformWidget(
			Selection &selection,
			Ogre::SceneManager &sceneMgr,
			Ogre::Camera &camera
			)
			: m_mode(transform_mode::Translate)
			, m_isLocal(true)
			, m_step(1.0f)
			, m_selectedAxis(axis_id::None)
			, m_selection(selection)
			, m_snappedTranslation(0.0f, 0.0f, 0.0f)
			, m_dummyCamera(nullptr)
			, m_relativeWidgetPos(0.0f, 0.0f, 0.0f)
			, m_widgetNode(nullptr)
			, m_translationNode(nullptr)
			, m_rotationNode(nullptr)
			, m_scaleNode(nullptr)
			, m_sceneMgr(sceneMgr)
			, m_camera(camera)
			, m_active(false)
			, m_lastMouse(0, 0)
			, m_lastIntersection(0.0f, 0.0f, 0.0f)
			, m_translation(0.0f, 0.0f, 0.0f)
			, m_rotation(1.0f, 0.0f, 0.0f, 0.0f)
			, m_scale(1.0f)
			, m_keyDown(false)
			, m_resetMouse(false)
			, m_copyMode(false)
			, m_sleep(false)
			, m_camDir(0.0f, 0.0f, 0.0f)
			, m_visible(true)
			, m_axisLines(nullptr)
			, m_xArrowNode(nullptr)
			, m_yArrowNode(nullptr)
			, m_zArrowNode(nullptr)
			, m_xArrow(nullptr)
			, m_yArrow(nullptr)
			, m_zArrow(nullptr)
			, m_xzPlaneNode(nullptr)
			, m_xyPlaneNode(nullptr)
			, m_yzPlaneNode(nullptr)
			, m_zRotNode(nullptr)
			, m_xRotNode(nullptr)
			, m_yRotNode(nullptr)
			, m_xCircle(nullptr)
			, m_yCircle(nullptr)
			, m_zCircle(nullptr)
			, m_fullCircleEntity(nullptr)
			, m_rotationCenter(nullptr)
		{
			// Create central node for all transformation modes
			m_widgetNode = m_sceneMgr.getRootSceneNode()->createChildSceneNode();

			// Create and setup dummy cam
			m_dummyCamera = m_sceneMgr.createCamera("DummyCam-" + m_widgetNode->getName());
			m_dummyCamera->setOrientation(camera.getRealOrientation());
			m_dummyCamera->setAspectRatio(camera.getAspectRatio());
			m_dummyCamera->setProjectionType(camera.getProjectionType());
			m_dummyCamera->setNearClipDistance(camera.getNearClipDistance());
			m_dummyCamera->setFarClipDistance(camera.getFarClipDistance());

			// Translation-Mode initialization
			setupTranslation();

			// Rotation-Mode initialization
			setupRotation();

			// Scale-Mode initialization
			setupScale();

			// Hide until something is selected
			m_widgetNode->setVisible(false);

			// Watch for selection changes
			m_onSelectionChanged = m_selection.changed.connect(this, &TransformWidget::onSelectionChanged);
		}

		TransformWidget::~TransformWidget()
		{
			if (m_rotationCenter)
			{
				m_sceneMgr.destroySceneNode(m_rotationCenter);
				m_rotationCenter = nullptr;
			}

			if (m_widgetNode)
			{
				m_widgetNode->removeAndDestroyAllChildren();
			}

			m_sceneMgr.destroyManualObject(m_axisLines);
			m_sceneMgr.destroyEntity(m_xArrow);
			m_sceneMgr.destroyEntity(m_yArrow);
			m_sceneMgr.destroyEntity(m_zArrow);
			m_sceneMgr.destroyEntity(m_xCircle);
			m_sceneMgr.destroyEntity(m_yCircle);
			m_sceneMgr.destroyEntity(m_zCircle);
			m_sceneMgr.destroyEntity(m_fullCircleEntity);
			m_sceneMgr.destroyCamera(m_dummyCamera);
			if (m_widgetNode)
			{
				m_sceneMgr.destroySceneNode(m_widgetNode);
			}
		}

		void TransformWidget::update(Ogre::Camera *camera)
		{
			if (!m_active)
			{
				if (camera->getProjectionType() == Ogre::PT_PERSPECTIVE)
				{
					// Scale with camera distance
					const Ogre::Vector3 distance = m_widgetNode->getPosition() - camera->getDerivedPosition();
					m_scale = distance.length() * 0.15f;
					m_widgetNode->setScale(m_scale, m_scale, m_scale);
				}
				else
				{
					m_widgetNode->setScale(m_scale, m_scale, m_scale);
				}
			}

			// Flip the rotation widget depending on the camera position
			switch (m_mode)
			{
				case transform_mode::Rotate:
				{
					m_camDir = m_camera.getRealPosition() - m_widgetNode->getPosition();
					m_camDir = m_widgetNode->getOrientation().Inverse() * m_camDir;

					Ogre::Vector3 xScale(1.0f, 1.0f, 1.0f);
					Ogre::Vector3 yScale(1.0f, 1.0f, 1.0f);
					Ogre::Vector3 zScale(1.0f, 1.0f, 1.0f);

					if (m_camDir.y < 0.0f)
					{
						xScale.y = -1.0f;
						zScale.y = -1.0f;
					}

					if (m_camDir.x < 0.0f)
					{
						yScale.x = -1.0f;
						zScale.x = -1.0f;
					}

					if (m_camDir.z < 0.0f)
					{
						xScale.x = -1.0f;
						yScale.y = -1.0f;
					}

					m_xRotNode->setScale(xScale);
					m_yRotNode->setScale(yScale);
					m_zRotNode->setScale(zScale);
				} break;

				case transform_mode::Scale:
				{
					m_camDir = m_camera.getRealPosition() - m_widgetNode->getPosition();
					m_camDir = m_widgetNode->getOrientation().Inverse() * m_camDir;

					Ogre::Vector3 scale(1.0f, 1.0f, 1.0f);

					if (m_camDir.y < 0.0f)
					{
						scale.y = -1.0f;
					}
					if (m_camDir.x < 0.0f)
					{
						scale.x = -1.0f;
					}
					if (m_camDir.z < 0.0f)
					{
						scale.z = -1.0f;
					}

					m_scaleNode->setScale(scale);
				} break;
			}

			// Don't update the looks of the widget if nothing is selected or the user is flying around
			if (!m_selection.empty())
			{
				switch (m_mode)
				{
					case transform_mode::Translate:
					{
						updateTranslation();
						break;
					}

					case transform_mode::Rotate:
					{
						updateRotation();
						break;
					}

					case transform_mode::Scale:
					{
						updateScale();
						break;
					}
				}
			}

			// Update dummy camera
			m_dummyCamera->setOrientation(m_camera.getRealOrientation());
			m_dummyCamera->setAspectRatio(m_camera.getAspectRatio());
			m_relativeWidgetPos = m_widgetNode->getPosition() - m_camera.getRealPosition();
		}

		void TransformWidget::onKeyPressed(const QKeyEvent *event)
		{
			if (!m_visible)
			{
				return;
			}

			Ogre::Vector3 dir(0.0f, 0.0f, 0.0f);
			switch (event->key())
			{
				case Qt::Key_Up:
				{
					dir = Ogre::Vector3::NEGATIVE_UNIT_Z;
					break;
				}

				case Qt::Key_Down:
				{
					dir = Ogre::Vector3::UNIT_Z;
					break;
				}

				case Qt::Key_Left:
				{
					dir = Ogre::Vector3::NEGATIVE_UNIT_X;
					break;
				}

				case Qt::Key_Right:
				{
					dir = Ogre::Vector3::UNIT_X;
					break;
				}

				case Qt::Key_PageUp:
				{
					dir = Ogre::Vector3::UNIT_Y;
					break;
				}

				case Qt::Key::Key_PageDown:
				{
					dir = Ogre::Vector3::NEGATIVE_UNIT_Y;
					break;
				}

				case Qt::Key_Shift:
				{
					if (!m_active)
					{
						m_copyMode = true;
					}
					break;
				}
			}

			if (dir.length() == 0)
			{
				return;
			}

			if (!m_keyDown)
			{
				m_keyDown = true;
				m_translation = Ogre::Vector3::ZERO;
			}

			dir *= m_step;
			dir = m_camera.getRealOrientation() * dir;

			switch (m_mode)
			{
				case transform_mode::Translate:
				{
					applyTranslation(dir);
					break;
				}
			}
		}

		void TransformWidget::onKeyReleased(const QKeyEvent *event)
		{
			if (event->key() == Qt::Key_Shift)
			{
				m_copyMode = false;
			}

			if (m_keyDown)
			{
				m_keyDown = false;
				switch (m_mode)
				{
					case transform_mode::Translate:
					{
						finishTranslation();
						break;
					}
				}
			}
		}

		void TransformWidget::onMousePressed(const QMouseEvent *event)
		{
			if (event->button() == Qt::LeftButton)
			{
				if (m_selectedAxis != axis_id::None && !m_sleep && m_visible)
				{
					m_active = true;

					switch (m_mode)
					{
						case transform_mode::Translate:
						{
							m_translation = Ogre::Vector3::ZERO;

							Ogre::Plane plane = getTranslatePlane(m_selectedAxis);
							Ogre::Viewport *viewport = m_camera.getViewport();
							Ogre::Ray ray = m_dummyCamera->getCameraToViewportRay(
								static_cast<float>(event->localPos().x()) / static_cast<float>(viewport->getActualWidth()),
								static_cast<float>(event->localPos().y()) / static_cast<float>(viewport->getActualHeight()));
							auto res = ray.intersects(plane);
							if (res.first)
							{
								m_lastIntersection = ray.getPoint(res.second);
							}

							break;
						}

						case transform_mode::Rotate:
						{
							m_rotation = Ogre::Quaternion::IDENTITY;
							break;
						}

						case transform_mode::Scale:
						{
							//TODO
							break;
						}
					}
				}
			}
		}

		void TransformWidget::onMouseReleased(const QMouseEvent *event)
		{
			if (event->button() == Qt::LeftButton)
			{
				m_active = false;

				switch (m_mode)
				{
					case transform_mode::Translate:
					{
						finishTranslation();
						break;
					}

					case transform_mode::Rotate:
					{
						finishRotation();
						break;
					}

					case transform_mode::Scale:
					{
						finishScale();
						break;
					}
				}
			}
		}

		void TransformWidget::onMouseMoved(const QMouseEvent *event)
		{
			if (!m_selection.empty() && 
				!m_sleep && 
				m_visible &&
				(event->localPos().x() - m_lastMouse[0] != 0 || event->localPos().y() - m_lastMouse[1] != 0))
			{
				if (m_copyMode && m_active)
				{
					m_copyMode = false;
					//ProjectManager.CopySelectedObjects();
				}

				Ogre::Viewport *viewport = m_camera.getViewport();
				Ogre::Ray ray = m_dummyCamera->getCameraToViewportRay(
					static_cast<float>(event->localPos().x()) / static_cast<float>(viewport->getActualWidth()),
					static_cast<float>(event->localPos().y()) / static_cast<float>(viewport->getActualHeight()));

				switch (m_mode)
				{
					case transform_mode::Translate:
					{
						translationMouseMoved(ray, event);
						break;
					}

					case transform_mode::Rotate:
					{
						rotationMouseMoved(ray, event);
						break;
					}

					case transform_mode::Scale:
					{
						scaleMouseMoved(ray, event);
						break;
					}
				}
			}

			m_lastMouse[0] = event->localPos().x();
			m_lastMouse[1] = event->localPos().y();
		}

		const bool TransformWidget::isActive() const
		{
			return m_active;
		}

		void TransformWidget::setTransformMode(TransformMode mode)
		{
			if (m_mode == mode)
			{
				return;
			}

			m_mode = mode;
			setVisibility();
		}

		void TransformWidget::setVisible(bool visible)
		{
			m_visible = visible;
			setVisibility();
		}

		void TransformWidget::setupTranslation()
		{
			// Setup axis lines
			m_axisLines = m_sceneMgr.createManualObject();
			m_axisLines->setQueryFlags(0);
			m_axisLines->setRenderQueueGroup(Ogre::RENDER_QUEUE_OVERLAY - 1);
			m_axisLines->setCastShadows(false);

			const Ogre::Vector3 xTipPos(CenterOffset + LineLength, 0.0f, 0.0f);
			const Ogre::Vector3 yTipPos(0.0f, CenterOffset + LineLength, 0.0f);
			const Ogre::Vector3 zTipPos(0.0f, 0.0f, CenterOffset + LineLength);

			// X Axis
			m_axisLines->begin("Editor/AxisX", Ogre::RenderOperation::OT_LINE_LIST);
			m_axisLines->position(CenterOffset, 0.0f, 0.0f);
			m_axisLines->position(xTipPos);
			m_axisLines->end();

			m_axisLines->begin("Editor/AxisX", Ogre::RenderOperation::OT_LINE_LIST);
			m_axisLines->position(SquareLength, 0.0f, 0.0f);
			m_axisLines->position(SquareLength, SquareLength, 0.0f);
			m_axisLines->end();

			m_axisLines->begin("Editor/AxisX", Ogre::RenderOperation::OT_LINE_LIST);
			m_axisLines->position(SquareLength, 0.0f, 0.0f);
			m_axisLines->position(SquareLength, 0.0f, SquareLength);
			m_axisLines->end();

			// Y Axis
			m_axisLines->begin("Editor/AxisY", Ogre::RenderOperation::OT_LINE_LIST);
			m_axisLines->position(0.0f, CenterOffset, 0.0f);
			m_axisLines->position(yTipPos);
			m_axisLines->end();

			m_axisLines->begin("Editor/AxisY", Ogre::RenderOperation::OT_LINE_LIST);
			m_axisLines->position(0.0f, SquareLength, 0.0f);
			m_axisLines->position(SquareLength, SquareLength, 0.0f);
			m_axisLines->end();

			m_axisLines->begin("Editor/AxisY", Ogre::RenderOperation::OT_LINE_LIST);
			m_axisLines->position(0.0f, SquareLength, 0.0f);
			m_axisLines->position(0.0f, SquareLength, SquareLength);
			m_axisLines->end();

			// Z Axis
			m_axisLines->begin("Editor/AxisZ", Ogre::RenderOperation::OT_LINE_LIST);
			m_axisLines->position(0.0f, 0.0f, CenterOffset);
			m_axisLines->position(zTipPos);
			m_axisLines->end();

			m_axisLines->begin("Editor/AxisZ", Ogre::RenderOperation::OT_LINE_LIST);
			m_axisLines->position(0.0f, 0.0f, SquareLength);
			m_axisLines->position(0.0f, SquareLength, SquareLength);
			m_axisLines->end();

			m_axisLines->begin("Editor/AxisZ", Ogre::RenderOperation::OT_LINE_LIST);
			m_axisLines->position(0.0f, 0.0f, SquareLength);
			m_axisLines->position(SquareLength, 0.0f, SquareLength);
			m_axisLines->end();

			// Create translation node
			m_translationNode = m_widgetNode->createChildSceneNode();
			m_translationNode->attachObject(m_axisLines);

			// Setup arrows
			m_xArrowNode = m_translationNode->createChildSceneNode();
			m_xArrowNode->setPosition(xTipPos);
			m_yArrowNode = m_translationNode->createChildSceneNode();
			m_yArrowNode->setPosition(yTipPos);
			m_zArrowNode = m_translationNode->createChildSceneNode();
			m_zArrowNode->setPosition(zTipPos);
			
			// X Arrow
			m_xArrow = m_sceneMgr.createEntity(ArrowMeshName);
			m_xArrow->setRenderQueueGroup(Ogre::RENDER_QUEUE_OVERLAY - 1);
			m_xArrow->setQueryFlags(0);
			m_xArrow->setMaterialName("Editor/AxisX");
			m_xArrow->setCastShadows(false);
			m_xArrowNode->attachObject(m_xArrow);
			m_xArrowNode->yaw(Ogre::Degree(90.0f));

			// Y Arrow
			m_yArrow = m_sceneMgr.createEntity(ArrowMeshName);
			m_yArrow->setRenderQueueGroup(Ogre::RENDER_QUEUE_OVERLAY - 1);
			m_yArrow->setQueryFlags(0);
			m_yArrow->setMaterialName("Editor/AxisY");
			m_yArrow->setCastShadows(false);
			m_yArrowNode->attachObject(m_yArrow);
			m_yArrowNode->pitch(Ogre::Degree(-90.0f));

			// Z Arrow
			m_zArrow = m_sceneMgr.createEntity(ArrowMeshName);
			m_zArrow->setRenderQueueGroup(Ogre::RENDER_QUEUE_OVERLAY - 1);
			m_zArrow->setQueryFlags(0);
			m_zArrow->setMaterialName("Editor/AxisZ");
			m_zArrow->setCastShadows(false);
			m_zArrowNode->attachObject(m_zArrow);

			// Setup axis
			if (!Ogre::MeshManager::getSingleton().resourceExists(AxisPlaneName))
			{
				Ogre::ManualObject *planeObj = m_sceneMgr.createManualObject();
				const Ogre::ColourValue planeColour(1.0f, 1.0f, 0.0f, 0.3f);
				planeObj->begin("Editor/AxisPlane", Ogre::RenderOperation::OT_TRIANGLE_STRIP);
				planeObj->position(SquareLength, 0.0f, 0.0f);
				planeObj->colour(planeColour);
				planeObj->position(0.0f, 0.0f, 0.0f);
				planeObj->colour(planeColour);
				planeObj->position(SquareLength, 0.0f, SquareLength);
				planeObj->colour(planeColour);
				planeObj->position(0.0f, 0.0f, SquareLength);
				planeObj->colour(planeColour);
				planeObj->end();

				m_translateAxisPlanes = planeObj->convertToMesh(AxisPlaneName);
				m_sceneMgr.destroyManualObject(planeObj);
			}
			else
			{
				m_translateAxisPlanes = Ogre::MeshManager::getSingleton().getByName(AxisPlaneName);
			}

			// Create on node for each plane and rotate
			m_xzPlaneNode = m_translationNode->createChildSceneNode();
			m_xyPlaneNode = m_translationNode->createChildSceneNode();
			m_xyPlaneNode->pitch(Ogre::Degree(-90.0f));
			m_yzPlaneNode = m_translationNode->createChildSceneNode();
			m_yzPlaneNode->roll(Ogre::Degree(90.0f));

			Ogre::Entity *plane1 = m_sceneMgr.createEntity(AxisPlaneName);
			plane1->setQueryFlags(0);
			plane1->setRenderQueueGroup(Ogre::RENDER_QUEUE_OVERLAY - 1);
			plane1->setCastShadows(false);
			Ogre::Entity *plane2 = m_sceneMgr.createEntity(AxisPlaneName);
			plane2->setQueryFlags(0);
			plane2->setRenderQueueGroup(Ogre::RENDER_QUEUE_OVERLAY - 1);
			plane2->setCastShadows(false);
			Ogre::Entity *plane3 = m_sceneMgr.createEntity(AxisPlaneName);
			plane3->setQueryFlags(0);
			plane3->setRenderQueueGroup(Ogre::RENDER_QUEUE_OVERLAY - 1);
			plane3->setCastShadows(false);

			m_xzPlaneNode->attachObject(plane1);
			m_xyPlaneNode->attachObject(plane2);
			m_yzPlaneNode->attachObject(plane3);

			// Hide them initially
			m_xzPlaneNode->setVisible(false);
			m_xyPlaneNode->setVisible(false);
			m_yzPlaneNode->setVisible(false);
		}

		void TransformWidget::setupRotation()
		{
			// Create one node for all rotation objects
			m_rotationNode = m_widgetNode->createChildSceneNode();
			m_rotationNode->setVisible(false);

			const size_t numSteps = 12;
			const Ogre::ColourValue fillColor(1.0f, 1.0f, 1.0f, 0.3f);

			// Create circle mesh
			if (!Ogre::MeshManager::getSingleton().resourceExists(CircleMeshName))
			{
				Ogre::ManualObject *circles = m_sceneMgr.createManualObject();
				const float endAngle = Ogre::Math::PI / 2.0f;

				// Outer circle
				circles->begin("Editor/AxisX", Ogre::RenderOperation::OT_LINE_STRIP);
				for (float angle = 0.0f; angle < endAngle; angle += endAngle / numSteps)
				{
					circles->position(
						OuterRadius * Ogre::Math::Cos(angle),
						OuterRadius * Ogre::Math::Sin(angle),
						0.0f);
				}
				circles->position(0.0f, OuterRadius, 0.0f);
				circles->end();

				// Inner circle
				circles->begin("Editor/AxisX", Ogre::RenderOperation::OT_LINE_STRIP);
				for (float angle = 0.0f; angle < endAngle; angle += endAngle / numSteps)
				{
					circles->position(
						InnerRadius * Ogre::Math::Cos(angle),
						InnerRadius * Ogre::Math::Sin(angle),
						0.0f);
				}
				circles->position(0.0f, InnerRadius, 0.0f);
				circles->end();

				// Fill circle
				circles->begin("Editor/AxisX", Ogre::RenderOperation::OT_TRIANGLE_STRIP);
				for (float angle = 0.0f; angle < endAngle; angle += endAngle / numSteps)
				{
					circles->position(OuterRadius * Ogre::Math::Cos(angle), OuterRadius * Ogre::Math::Sin(angle), 0.0f);
					circles->colour(fillColor);
					circles->position(InnerRadius * Ogre::Math::Cos(angle), InnerRadius * Ogre::Math::Sin(angle), 0.0f);
					circles->colour(fillColor);
				}
				circles->position(0.0f, OuterRadius, 0.0f);
				circles->colour(fillColor);
				circles->position(0.0f, InnerRadius, 0.0f);
				circles->colour(fillColor);
				circles->end();

				m_circleMesh = circles->convertToMesh(CircleMeshName);
				m_sceneMgr.destroyManualObject(circles);
			}
			else
			{
				m_circleMesh = Ogre::MeshManager::getSingleton().getByName(CircleMeshName);
			}
			
			if (!Ogre::MeshManager::getSingleton().resourceExists(FullCircleMeshName))
			{
				Ogre::ManualObject *fullCircle = m_sceneMgr.createManualObject();
				const float endAngle2 = 2.0f * Ogre::Math::PI;

				// Outer circle
				fullCircle->begin("Editor/AxisX", Ogre::RenderOperation::OT_LINE_STRIP);
				for (float angle = 0.0f; angle < endAngle2; angle += endAngle2 / numSteps)
				{
					fullCircle->position(
						OuterRadius * Ogre::Math::Cos(angle),
						OuterRadius * Ogre::Math::Sin(angle),
						0.0f);
				}
				fullCircle->position(0.0f, OuterRadius, 0.0f);
				fullCircle->end();

				// Inner circle
				fullCircle->begin("Editor/AxisX", Ogre::RenderOperation::OT_LINE_STRIP);
				for (float angle = 0.0f; angle < endAngle2; angle += endAngle2 / numSteps)
				{
					fullCircle->position(
						InnerRadius * Ogre::Math::Cos(angle),
						InnerRadius * Ogre::Math::Sin(angle),
						0.0f);
				}
				fullCircle->position(0.0f, InnerRadius, 0.0f);
				fullCircle->end();

				// Fill circle
				fullCircle->begin("Editor/AxisX", Ogre::RenderOperation::OT_TRIANGLE_STRIP);
				for (float angle = 0.0f; angle < endAngle2; angle += endAngle2 / numSteps)
				{
					fullCircle->position(OuterRadius * Ogre::Math::Cos(angle), OuterRadius * Ogre::Math::Sin(angle), 0.0f);
					fullCircle->colour(fillColor);
					fullCircle->position(InnerRadius * Ogre::Math::Cos(angle), InnerRadius * Ogre::Math::Sin(angle), 0.0f);
					fullCircle->colour(fillColor);
				}
				fullCircle->position(0.0f, OuterRadius, 0.0f);
				fullCircle->colour(fillColor);
				fullCircle->position(0.0f, InnerRadius, 0.0f);
				fullCircle->colour(fillColor);
				fullCircle->end();

				m_fullCircleMesh = fullCircle->convertToMesh(FullCircleMeshName);
				m_sceneMgr.destroyManualObject(fullCircle);
			}
			else
			{
				m_fullCircleMesh = Ogre::MeshManager::getSingleton().getByName(FullCircleMeshName);
			}

			// Create circle entities and attach to scene
			m_zRotNode = m_rotationNode->createChildSceneNode();
			m_xRotNode = m_rotationNode->createChildSceneNode();
			m_xRotNode->yaw(Ogre::Degree(-90.0f));
			m_yRotNode = m_rotationNode->createChildSceneNode();
			m_yRotNode->pitch(Ogre::Degree(90.0f));

			m_fullCircleEntity = m_sceneMgr.createEntity(FullCircleMeshName);
			m_fullCircleEntity->setQueryFlags(0);
			m_fullCircleEntity->setRenderQueueGroup(Ogre::RENDER_QUEUE_OVERLAY - 1);
			m_fullCircleEntity->setCastShadows(false);

			m_xCircle = m_sceneMgr.createEntity(CircleMeshName);
			m_xCircle->setQueryFlags(0);
			m_xCircle->setRenderQueueGroup(Ogre::RENDER_QUEUE_OVERLAY - 1);
			m_xCircle->setMaterialName("Editor/AxisX");
			m_xCircle->setCastShadows(false);

			m_yCircle = m_sceneMgr.createEntity(CircleMeshName);
			m_yCircle->setQueryFlags(0);
			m_yCircle->setRenderQueueGroup(Ogre::RENDER_QUEUE_OVERLAY - 1);
			m_yCircle->setMaterialName("Editor/AxisY");
			m_yCircle->setCastShadows(false);

			m_zCircle = m_sceneMgr.createEntity(CircleMeshName);
			m_zCircle->setQueryFlags(0);
			m_zCircle->setRenderQueueGroup(Ogre::RENDER_QUEUE_OVERLAY - 1);
			m_zCircle->setMaterialName("Editor/AxisZ");
			m_zCircle->setCastShadows(false);

			m_xRotNode->attachObject(m_xCircle);
			m_yRotNode->attachObject(m_yCircle);
			m_zRotNode->attachObject(m_zCircle);
		}

		void TransformWidget::setupScale()
		{
			// Setup axis lines
			m_scaleAxisLines = m_sceneMgr.createManualObject();
			m_scaleAxisLines->setQueryFlags(0);
			m_scaleAxisLines->setRenderQueueGroup(Ogre::RENDER_QUEUE_OVERLAY - 1);
			m_scaleAxisLines->setCastShadows(false);

			const Ogre::Vector3 xTipPos(LineLength, 0.0f, 0.0f);
			const Ogre::Vector3 yTipPos(0.0f, LineLength, 0.0f);
			const Ogre::Vector3 zTipPos(0.0f, 0.0f, LineLength);

			// X Axis
			m_scaleAxisLines->begin("Editor/AxisX", Ogre::RenderOperation::OT_LINE_LIST);
			m_scaleAxisLines->position(0.0f, 0.0f, 0.0f);
			m_scaleAxisLines->position(xTipPos);
			m_scaleAxisLines->end();

			m_scaleAxisLines->begin("Editor/AxisX", Ogre::RenderOperation::OT_LINE_LIST);
			m_scaleAxisLines->position(xTipPos.x * 0.5f, 0.0f, 0.0f);
			m_scaleAxisLines->position(xTipPos.x * 0.25f, yTipPos.y * 0.25f, 0.0f);
			m_scaleAxisLines->end();

			m_scaleAxisLines->begin("Editor/AxisX", Ogre::RenderOperation::OT_LINE_LIST);
			m_scaleAxisLines->position(xTipPos.x * 0.5f, 0.0f, 0.0f);
			m_scaleAxisLines->position(xTipPos.x * 0.25f, 0.0f, zTipPos.z * 0.25f);
			m_scaleAxisLines->end();

			m_scaleAxisLines->begin("Editor/AxisX", Ogre::RenderOperation::OT_LINE_LIST);
			m_scaleAxisLines->position(xTipPos.x * 0.75f, 0.0f, 0.0f);
			m_scaleAxisLines->position(xTipPos.x * 0.375f, yTipPos.y * 0.375f, 0.0f);
			m_scaleAxisLines->end();

			m_scaleAxisLines->begin("Editor/AxisX", Ogre::RenderOperation::OT_LINE_LIST);
			m_scaleAxisLines->position(xTipPos.x * 0.75f, 0.0f, 0.0f);
			m_scaleAxisLines->position(xTipPos.x * 0.375f, 0.0f, zTipPos.z * 0.375f);
			m_scaleAxisLines->end();

			// Y Axis
			m_scaleAxisLines->begin("Editor/AxisY", Ogre::RenderOperation::OT_LINE_LIST);
			m_scaleAxisLines->position(0.0f, 0.0f, 0.0f);
			m_scaleAxisLines->position(yTipPos);
			m_scaleAxisLines->end();

			m_scaleAxisLines->begin("Editor/AxisY", Ogre::RenderOperation::OT_LINE_LIST);
			m_scaleAxisLines->position(0.0f, yTipPos.y * 0.5f, 0.0f);
			m_scaleAxisLines->position(xTipPos.x * 0.25f, yTipPos.y * 0.25f, 0.0f);
			m_scaleAxisLines->end();

			m_scaleAxisLines->begin("Editor/AxisY", Ogre::RenderOperation::OT_LINE_LIST);
			m_scaleAxisLines->position(0.0f, yTipPos.y * 0.5f, 0.0f);
			m_scaleAxisLines->position(0.0f, yTipPos.y * 0.25f, zTipPos.z * 0.25f);
			m_scaleAxisLines->end();

			m_scaleAxisLines->begin("Editor/AxisY", Ogre::RenderOperation::OT_LINE_LIST);
			m_scaleAxisLines->position(0.0f, yTipPos.y * 0.75f, 0.0f);
			m_scaleAxisLines->position(xTipPos.x * 0.375f, yTipPos.y * 0.375f, 0.0f);
			m_scaleAxisLines->end();

			m_scaleAxisLines->begin("Editor/AxisY", Ogre::RenderOperation::OT_LINE_LIST);
			m_scaleAxisLines->position(0.0f, yTipPos.y * 0.75f, 0.0f);
			m_scaleAxisLines->position(0.0f, yTipPos.y * 0.375f, zTipPos.z * 0.375f);
			m_scaleAxisLines->end();

			// Z Axis
			m_scaleAxisLines->begin("Editor/AxisZ", Ogre::RenderOperation::OT_LINE_LIST);
			m_scaleAxisLines->position(0.0f, 0.0f, 0.0f);
			m_scaleAxisLines->position(zTipPos);
			m_scaleAxisLines->end();

			m_scaleAxisLines->begin("Editor/AxisZ", Ogre::RenderOperation::OT_LINE_LIST);
			m_scaleAxisLines->position(0.0f, 0.0f, zTipPos.z * 0.5f);
			m_scaleAxisLines->position(0.0f, yTipPos.y * 0.25f, zTipPos.z * 0.25f);
			m_scaleAxisLines->end();

			m_scaleAxisLines->begin("Editor/AxisZ", Ogre::RenderOperation::OT_LINE_LIST);
			m_scaleAxisLines->position(0.0f, 0.0f, zTipPos.z * 0.5f);
			m_scaleAxisLines->position(xTipPos.x * 0.25f, 0.0f, zTipPos.z * 0.25f);
			m_scaleAxisLines->end();

			m_scaleAxisLines->begin("Editor/AxisZ", Ogre::RenderOperation::OT_LINE_LIST);
			m_scaleAxisLines->position(0.0f, 0.0f, zTipPos.z * 0.75f);
			m_scaleAxisLines->position(0.0f, yTipPos.y * 0.375f, zTipPos.z * 0.375f);
			m_scaleAxisLines->end();

			m_scaleAxisLines->begin("Editor/AxisZ", Ogre::RenderOperation::OT_LINE_LIST);
			m_scaleAxisLines->position(0.0f, 0.0f, zTipPos.z * 0.75f);
			m_scaleAxisLines->position(xTipPos.x * 0.375f, 0.0f, zTipPos.z * 0.375f);
			m_scaleAxisLines->end();

			// Create translation node
			m_scaleNode = m_widgetNode->createChildSceneNode();
			m_scaleNode->attachObject(m_scaleAxisLines);

			if (!Ogre::MeshManager::getSingleton().resourceExists(ScaleContentPlaneName))
			{
				Ogre::ManualObject *planeObj = m_sceneMgr.createManualObject();
				const Ogre::ColourValue planeColour(1.0f, 1.0f, 0.0f, 0.3f);
				planeObj->begin("Editor/AxisPlane", Ogre::RenderOperation::OT_TRIANGLE_FAN);
				planeObj->position(0.0f, 0.0f, 0.0f);
				planeObj->colour(planeColour);
				planeObj->position(SquareLength, 0.0f, 0.0f);
				planeObj->colour(planeColour);
				planeObj->position(0.0f, 0.0f, SquareLength);
				planeObj->colour(planeColour);
				planeObj->position(0.0f, SquareLength, 0.0f);
				planeObj->colour(planeColour);
				planeObj->position(SquareLength, 0.0f, 0.0f);
				planeObj->colour(planeColour);
				planeObj->end();

				m_scaleCenterPlanes = planeObj->convertToMesh(ScaleContentPlaneName);
				m_sceneMgr.destroyManualObject(planeObj);
			}
			else
			{
				m_scaleCenterPlanes = Ogre::MeshManager::getSingleton().getByName(ScaleContentPlaneName);
			}

			if (!Ogre::MeshManager::getSingleton().resourceExists(ScaleAxisPlaneName))
			{
				Ogre::ManualObject *planeObj = m_sceneMgr.createManualObject();
				const Ogre::ColourValue planeColour(1.0f, 1.0f, 0.0f, 0.3f);
				planeObj->begin("Editor/AxisPlane", Ogre::RenderOperation::OT_TRIANGLE_STRIP);
				planeObj->position(LineLength * 0.75f, 0.0f, 0.0f);
				planeObj->colour(planeColour);
				planeObj->position(LineLength * 0.5f, 0.0f, 0.0f);
				planeObj->colour(planeColour);
				planeObj->position(0.0f, 0.0f, LineLength * 0.75f);
				planeObj->colour(planeColour);
				planeObj->position(0.0f, 0.0f, LineLength * 0.5f);
				planeObj->colour(planeColour);
				planeObj->end();

				m_scaleAxisPlanes = planeObj->convertToMesh(ScaleAxisPlaneName);
				m_sceneMgr.destroyManualObject(planeObj);
			}
			else
			{
				m_scaleAxisPlanes = Ogre::MeshManager::getSingleton().getByName(ScaleAxisPlaneName);
			}

			// Create the content node
			m_scaleContentPlaneNode = m_scaleNode->createChildSceneNode();

			Ogre::Entity *plane = m_sceneMgr.createEntity(ScaleContentPlaneName);
			plane->setQueryFlags(0);
			plane->setRenderQueueGroup(Ogre::RENDER_QUEUE_OVERLAY - 1);
			plane->setCastShadows(false);

			m_scaleContentPlaneNode->attachObject(plane);
			m_scaleContentPlaneNode->setVisible(false);

			// Create on node for each plane and rotate
			m_scaleXZPlaneNode = m_scaleNode->createChildSceneNode();
			m_scaleXYPlaneNode = m_scaleNode->createChildSceneNode();
			m_scaleXYPlaneNode->pitch(Ogre::Degree(-90.0f));
			m_scaleYZPlaneNode = m_scaleNode->createChildSceneNode();
			m_scaleYZPlaneNode->roll(Ogre::Degree(90.0f));

			Ogre::Entity *plane1 = m_sceneMgr.createEntity(ScaleAxisPlaneName);
			plane1->setQueryFlags(0);
			plane1->setRenderQueueGroup(Ogre::RENDER_QUEUE_OVERLAY - 1);
			plane1->setCastShadows(false);
			Ogre::Entity *plane2 = m_sceneMgr.createEntity(ScaleAxisPlaneName);
			plane2->setQueryFlags(0);
			plane2->setRenderQueueGroup(Ogre::RENDER_QUEUE_OVERLAY - 1);
			plane2->setCastShadows(false);
			Ogre::Entity *plane3 = m_sceneMgr.createEntity(ScaleAxisPlaneName);
			plane3->setQueryFlags(0);
			plane3->setRenderQueueGroup(Ogre::RENDER_QUEUE_OVERLAY - 1);
			plane3->setCastShadows(false);

			m_scaleXZPlaneNode->attachObject(plane1);
			m_scaleXYPlaneNode->attachObject(plane2);
			m_scaleYZPlaneNode->attachObject(plane3);

			// Hide them initially
			m_scaleXZPlaneNode->setVisible(false);
			m_scaleXYPlaneNode->setVisible(false);
			m_scaleYZPlaneNode->setVisible(false);
		}

		void TransformWidget::updateTranslation()
		{
			// Set x-axis color
			if ((m_selectedAxis & axis_id::X))
			{
				m_axisLines->setMaterialName(0, "Editor/AxisSelected");
			}
			else
			{
				m_axisLines->setMaterialName(0, "Editor/AxisX");
			}

			// Set y-axis color
			if ((m_selectedAxis & axis_id::Y))
			{
				m_axisLines->setMaterialName(3, "Editor/AxisSelected");
			}
			else
			{
				m_axisLines->setMaterialName(3, "Editor/AxisY");
			}

			// Set z-axis color
			if ((m_selectedAxis & axis_id::Z))
			{
				m_axisLines->setMaterialName(6, "Editor/AxisSelected");
			}
			else
			{
				m_axisLines->setMaterialName(6, "Editor/AxisZ");
			}

			// Display xz-Plane
			if ((m_selectedAxis & axis_id::X) && (m_selectedAxis & axis_id::Z))
			{
				m_xzPlaneNode->setVisible(true);
				m_axisLines->setMaterialName(2, "Editor/AxisSelected");
				m_axisLines->setMaterialName(8, "Editor/AxisSelected");
			}
			else
			{
				m_xzPlaneNode->setVisible(false);
				m_axisLines->setMaterialName(2, "Editor/AxisX");
				m_axisLines->setMaterialName(8, "Editor/AxisZ");
			}

			// Display xy-Plane
			if ((m_selectedAxis & axis_id::X) && (m_selectedAxis & axis_id::Y))
			{
				m_xyPlaneNode->setVisible(true);
				m_axisLines->setMaterialName(1, "Editor/AxisSelected");
				m_axisLines->setMaterialName(4, "Editor/AxisSelected");
			}
			else
			{
				m_xyPlaneNode->setVisible(false);
				m_axisLines->setMaterialName(1, "Editor/AxisX");
				m_axisLines->setMaterialName(4, "Editor/AxisY");
			}

			// Display yz-Plane
			if ((m_selectedAxis & axis_id::Y) && (m_selectedAxis & axis_id::Z))
			{
				m_yzPlaneNode->setVisible(true);
				m_axisLines->setMaterialName(5, "Editor/AxisSelected");
				m_axisLines->setMaterialName(7, "Editor/AxisSelected");
			}
			else
			{
				m_yzPlaneNode->setVisible(false);
				m_axisLines->setMaterialName(5, "Editor/AxisY");
				m_axisLines->setMaterialName(7, "Editor/AxisZ");
			}
		}

		void TransformWidget::updateRotation()
		{
			if ((m_selectedAxis & axis_id::X))
			{
				m_xCircle->setMaterialName("Editor/AxisSelected");
			}
			else
			{
				m_xCircle->setMaterialName("Editor/AxisX");
			}

			if ((m_selectedAxis & axis_id::Y))
			{
				m_yCircle->setMaterialName("Editor/AxisSelected");
			}
			else
			{
				m_yCircle->setMaterialName("Editor/AxisY");
			}

			if ((m_selectedAxis & axis_id::Z))
			{
				m_zCircle->setMaterialName("Editor/AxisSelected");
			}
			else
			{
				m_zCircle->setMaterialName("Editor/AxisZ");
			}
		}

		void TransformWidget::updateScale()
		{
			// Set x-axis color
			if ((m_selectedAxis & axis_id::X))
			{
				m_scaleAxisLines->setMaterialName(0, "Editor/AxisSelected");
			}
			else
			{
				m_scaleAxisLines->setMaterialName(0, "Editor/AxisX");
			}

			// Set y-axis color
			if ((m_selectedAxis & axis_id::Y))
			{
				m_scaleAxisLines->setMaterialName(5, "Editor/AxisSelected");
			}
			else
			{
				m_scaleAxisLines->setMaterialName(5, "Editor/AxisY");
			}

			// Set z-axis color
			if ((m_selectedAxis & axis_id::Z))
			{
				m_scaleAxisLines->setMaterialName(10, "Editor/AxisSelected");
			}
			else
			{
				m_scaleAxisLines->setMaterialName(10, "Editor/AxisZ");
			}

			// Display xz-Plane
			if (m_selectedAxis == axis_id::XZ)
			{
				m_scaleXZPlaneNode->setVisible(true);
				m_scaleAxisLines->setMaterialName(2, "Editor/AxisSelected");
				m_scaleAxisLines->setMaterialName(4, "Editor/AxisSelected");
				m_scaleAxisLines->setMaterialName(12, "Editor/AxisSelected");
				m_scaleAxisLines->setMaterialName(14, "Editor/AxisSelected");
			}
			else
			{
				m_scaleXZPlaneNode->setVisible(false);
				m_scaleAxisLines->setMaterialName(2, "Editor/AxisX");
				m_scaleAxisLines->setMaterialName(4, "Editor/AxisX");
				m_scaleAxisLines->setMaterialName(12, "Editor/AxisZ");
				m_scaleAxisLines->setMaterialName(14, "Editor/AxisZ");
			}

			// Display xy-Plane
			if (m_selectedAxis == axis_id::XY)
			{
				m_scaleXYPlaneNode->setVisible(true);
				m_scaleAxisLines->setMaterialName(1, "Editor/AxisSelected");
				m_scaleAxisLines->setMaterialName(3, "Editor/AxisSelected");
				m_scaleAxisLines->setMaterialName(6, "Editor/AxisSelected");
				m_scaleAxisLines->setMaterialName(8, "Editor/AxisSelected");
			}
			else
			{
				m_scaleXYPlaneNode->setVisible(false);
				m_scaleAxisLines->setMaterialName(1, "Editor/AxisX");
				m_scaleAxisLines->setMaterialName(3, "Editor/AxisX");
				m_scaleAxisLines->setMaterialName(6, "Editor/AxisY");
				m_scaleAxisLines->setMaterialName(8, "Editor/AxisY");
			}

			// Display yz-Plane
			if (m_selectedAxis == axis_id::YZ)
			{
				m_scaleYZPlaneNode->setVisible(true);
				m_scaleAxisLines->setMaterialName(7, "Editor/AxisSelected");
				m_scaleAxisLines->setMaterialName(9, "Editor/AxisSelected");
				m_scaleAxisLines->setMaterialName(11, "Editor/AxisSelected");
				m_scaleAxisLines->setMaterialName(13, "Editor/AxisSelected");
			}
			else
			{
				m_scaleYZPlaneNode->setVisible(false);
				m_scaleAxisLines->setMaterialName(7, "Editor/AxisY");
				m_scaleAxisLines->setMaterialName(9, "Editor/AxisY");
				m_scaleAxisLines->setMaterialName(11, "Editor/AxisZ");
				m_scaleAxisLines->setMaterialName(13, "Editor/AxisZ");
			}

			// All axes selected?
			if (m_selectedAxis == axis_id::All)
			{
				m_scaleContentPlaneNode->setVisible(true);
			}
			else
			{
				m_scaleContentPlaneNode->setVisible(false);
			}
		}

		void TransformWidget::changeMode()
		{

		}

		void TransformWidget::setVisibility()
		{
			if (!m_visible)
			{
				m_translationNode->setVisible(false);
				m_rotationNode->setVisible(false);
				m_scaleNode->setVisible(false);
				return;
			}

			if (!m_selection.empty())
			{
				switch (m_mode)
				{
					case transform_mode::Translate:
					{
						m_translationNode->setVisible(true);
						m_xyPlaneNode->setVisible(false);
						m_xzPlaneNode->setVisible(false);
						m_yzPlaneNode->setVisible(false);
						m_rotationNode->setVisible(false);
						m_scaleNode->setVisible(false);
						break;
					}

					case transform_mode::Rotate:
					{
						m_translationNode->setVisible(false);
						m_rotationNode->setVisible(true);
						m_scaleNode->setVisible(false);
						break;
					}

					case transform_mode::Scale:
					{
						m_scaleNode->setVisible(true);
						m_scaleXYPlaneNode->setVisible(false);
						m_scaleXZPlaneNode->setVisible(false);
						m_scaleYZPlaneNode->setVisible(false);
						m_translationNode->setVisible(false);
						m_rotationNode->setVisible(false);
						break;
					}
				}
			}
		}

		void TransformWidget::applyTranslation(const Ogre::Vector3 &dir)
		{
			m_translation += dir;

			for (auto &selected : m_selection.getSelectedObjects())
			{
				selected->translate(math::Vector3(dir.x, dir.y, dir.z));
			}
		}

		void TransformWidget::finishTranslation()
		{
			m_translation = Ogre::Vector3::ZERO;
		}

		void TransformWidget::applyRotation(const Ogre::Quaternion &rot)
		{
			m_rotation = rot * m_rotation;

// 			if (!m_isLocal)
// 			{
				// Rotate around own position
				for (auto &selected : m_selection.getSelectedObjects())
				{
					selected->rotate(
						Vector<float, 4>(rot.w, rot.x, rot.y, rot.z));
				}
// 			}
// 			else
// 			{
// 				// Temporarily attach all selected objects to rotation center
// 				if (m_rotationCenter == nullptr)
// 				{
// 					auto &position = m_selection.getSelectedObjects().back()->getPosition();
// 
// 					m_rotationCenter = m_sceneMgr.getRootSceneNode()->createChildSceneNode();
// 					m_rotationCenter->setPosition(
// 						position[0], position[1], position[2]);

// 					foreach(var obj in selectedObjs)
// 					{
// 						obj.SceneNode.Position = obj.SceneNode.Position - rotationCenter.Position;
// 						sceneMgr.RootSceneNode.RemoveChild(obj.SceneNode);
// 						rotationCenter.AddChild(obj.SceneNode);
// 					}
// 				}
// 
// 				m_rotationCenter->rotate(rot);
// 			}
		}

		void TransformWidget::finishRotation()
		{
			if (m_rotationCenter == nullptr)
			{
				m_rotation = Ogre::Quaternion::IDENTITY;
			}
			else
			{
				for (auto &obj : m_selection.getSelectedObjects())
				{
					/*obj.Position = obj.SceneNode._getDerivedPosition();
					obj.Orientation = obj.SceneNode._getDerivedOrientation();
					m_rotationCenter->removeChild(obj.SceneNode);
					m_sceneMgr.getRootSceneNode()->addChild(obj.SceneNode);*/
				}

				// Remove rotation center node
				m_sceneMgr.destroySceneNode(m_rotationCenter);
				m_rotationCenter = nullptr;
			}

			if (m_isLocal && 
				!m_selection.empty())
			{
				auto rot = m_selection.getSelectedObjects().back()->getOrientation();
				m_widgetNode->setOrientation(Ogre::Quaternion(rot[0], rot[1], rot[2], rot[3]));
			}
		}

		void TransformWidget::applyScale(const Ogre::Vector3 &dir)
		{
			// Rotate around own position
			for (auto &selected : m_selection.getSelectedObjects())
			{
				selected->scale(math::Vector3(dir.x, dir.y, dir.z));
			}

			m_scaleNode->scale(dir);
		}

		void TransformWidget::finishScale()
		{
			m_scaleNode->setScale(1.0f, 1.0f, 1.0f);
		}

		void TransformWidget::translationMouseMoved(Ogre::Ray &ray, const QMouseEvent *event)
		{
			if (!m_active)
			{
				m_selectedAxis = axis_id::None;

				// Check for intersection between mouse and translation widget
				auto res = ray.intersects(getTranslatePlane(AxisID(axis_id::X | axis_id::Z)));
				if (res.first)
				{
					Ogre::Vector3 dir = ray.getPoint(res.second) - m_relativeWidgetPos;
					dir = m_widgetNode->getOrientation().Inverse() * dir;
					if (dir.x > 0 && dir.x <= SquareLength * m_widgetNode->getScale().x && dir.z > 0 && dir.z <= SquareLength * m_widgetNode->getScale().x)
					{
						m_selectedAxis = AxisID(axis_id::X | axis_id::Z);
						return;
					}
				}
				if ((res = ray.intersects(getTranslatePlane(AxisID(axis_id::X | axis_id::Y)))).first)
				{
					Ogre::Vector3 dir = ray.getPoint(res.second) - m_relativeWidgetPos;
					dir = m_widgetNode->getOrientation().Inverse() * dir;
					if (dir.x > 0 && dir.x <= SquareLength * m_widgetNode->getScale().x && dir.y > 0 && dir.y <= SquareLength * m_widgetNode->getScale().x)
					{
						m_selectedAxis = AxisID(axis_id::X | axis_id::Y);
						return;
					}
				}
				if ((res = ray.intersects(getTranslatePlane(AxisID(axis_id::Y | axis_id::Z)))).first)
				{
					Ogre::Vector3 dir = ray.getPoint(res.second) - m_relativeWidgetPos;
					dir = m_widgetNode->getOrientation().Inverse() * dir;
					if (dir.z > 0 && dir.z <= SquareLength * m_widgetNode->getScale().x && dir.y > 0 && dir.y <= SquareLength * m_widgetNode->getScale().x)
					{
						m_selectedAxis = AxisID(axis_id::Y | axis_id::Z);
						return;
					}
				}
				if ((res = ray.intersects(getTranslatePlane(axis_id::X))).first)
				{
					Ogre::Vector3 dir = ray.getPoint(res.second) - m_relativeWidgetPos;
					dir = m_widgetNode->getOrientation().Inverse() * dir;
					if (dir.x > (CenterOffset * m_widgetNode->getScale().x) && dir.x <= (CenterOffset + LineLength + TipLength) * m_widgetNode->getScale().x)
					{
						Ogre::Vector3 projection = Ogre::Vector3::UNIT_X * dir.dotProduct(Ogre::Vector3::UNIT_X);
						Ogre::Vector3 difference = dir - projection;
						if (difference.length() < AxisBoxWidth * m_widgetNode->getScale().x)
						{
							m_selectedAxis = axis_id::X;
							return;
						}
					}
				}
				if ((res = ray.intersects(getTranslatePlane(axis_id::Y))).first)
				{
					Ogre::Vector3 dir = ray.getPoint(res.second) - m_relativeWidgetPos;
					dir = m_widgetNode->getOrientation().Inverse() * dir;
					if (dir.y > (CenterOffset * m_widgetNode->getScale().x) && dir.y <= (CenterOffset + LineLength + TipLength) * m_widgetNode->getScale().x)
					{
						Ogre::Vector3 projection = Ogre::Vector3::UNIT_Y * dir.dotProduct(Ogre::Vector3::UNIT_Y);
						Ogre::Vector3 difference = dir - projection;
						if (difference.length() < AxisBoxWidth * m_widgetNode->getScale().x)
						{
							m_selectedAxis = axis_id::Y;
							return;
						}
					}
				}
				if ((res = ray.intersects(getTranslatePlane(axis_id::Z))).first)
				{
					Ogre::Vector3 dir = ray.getPoint(res.second) - m_relativeWidgetPos;
					dir = m_widgetNode->getOrientation().Inverse() * dir;
					if (dir.z > (CenterOffset * m_widgetNode->getScale().x) && dir.z <= (CenterOffset + LineLength + TipLength) * m_widgetNode->getScale().x)
					{
						Ogre::Vector3 projection = Ogre::Vector3::UNIT_Z * dir.dotProduct(Ogre::Vector3::UNIT_Z);
						Ogre::Vector3 difference = dir - projection;
						if (difference.length() < AxisBoxWidth * m_widgetNode->getScale().x)
						{
							m_selectedAxis = axis_id::Z;
							return;
						}
					}
				}
			}
			else
			{
				// Translate
				if (!m_selection.empty())
				{
					Ogre::Vector3 distance(0.0f, 0.0f, 0.0f);

					Ogre::Vector3 direction(
						((m_selectedAxis & axis_id::X) ? 1.0f : 0.0f),
						((m_selectedAxis & axis_id::Y) ? 1.0f : 0.0f),
						((m_selectedAxis & axis_id::Z) ? 1.0f : 0.0f));

					Ogre::Plane plane = getTranslatePlane(m_selectedAxis);
					auto res = ray.intersects(plane);
					if (res.first)
					{
						Ogre::Vector3 intersection = ray.getPoint(res.second);
						distance = intersection - m_lastIntersection;
						distance = m_widgetNode->getOrientation().Inverse() * distance;
						distance *= direction;
						distance = m_widgetNode->getOrientation() * distance;
						m_lastIntersection = intersection;
					}

					applyTranslation(distance);
				}
			}
		}

		void TransformWidget::rotationMouseMoved(Ogre::Ray &ray, const QMouseEvent *event)
		{
			if (!m_active)
			{
				Ogre::Plane zRotPlane(m_widgetNode->getOrientation() * Ogre::Vector3::UNIT_Z, m_relativeWidgetPos);
				Ogre::Plane yRotPlane(m_widgetNode->getOrientation() * Ogre::Vector3::UNIT_Y, m_relativeWidgetPos);
				Ogre::Plane xRotPlane(m_widgetNode->getOrientation() * Ogre::Vector3::UNIT_X, m_relativeWidgetPos);

				auto zRes = ray.intersects(zRotPlane);
				auto yRes = ray.intersects(yRotPlane);
				auto xRes = ray.intersects(xRotPlane);

				bool xHit = false, yHit = false, zHit = false;
				const float bias = 0.1f;

				if (zRes.first)
				{
					Ogre::Vector3 zIntersection = ray.getPoint(zRes.second) - m_relativeWidgetPos;

					zHit = (zIntersection.length() <= ((OuterRadius + bias) * m_widgetNode->getScale().x))
						&& (zIntersection.length() >= ((InnerRadius - bias) * m_widgetNode->getScale().x));

					{
						zIntersection = m_widgetNode->getOrientation().Inverse() * zIntersection;
						zHit = zHit
							&& ((m_zRotNode->getScale().x * zIntersection.x) >= 0
							&& (m_zRotNode->getScale().y * zIntersection.y >= 0));
					}
				}
				if (yRes.first)
				{
					Ogre::Vector3 yIntersection = ray.getPoint(yRes.second) - m_relativeWidgetPos;

					yHit = (yIntersection.length() <= ((OuterRadius + bias) * m_widgetNode->getScale().x))
						&& (yIntersection.length() >= ((InnerRadius - bias) * m_widgetNode->getScale().x));

					{
						yIntersection = m_widgetNode->getOrientation().Inverse() * yIntersection;

						// Using y-scale for flipped z-axis because of rotation
						yHit = yHit
							&& ((m_yRotNode->getScale().x * yIntersection.x) >= 0
							&& (m_yRotNode->getScale().y * yIntersection.z >= 0));
					}
				}
				if (xRes.first)
				{
					Ogre::Vector3 xIntersection = ray.getPoint(xRes.second) - m_relativeWidgetPos;

					xHit = (xIntersection.length() <= ((OuterRadius + bias) * m_widgetNode->getScale().x))
						&& (xIntersection.length() >= ((InnerRadius - bias) * m_widgetNode->getScale().x));

					{
						xIntersection = m_widgetNode->getOrientation().Inverse() * xIntersection;

						// Using x-scale for flipped z-axis because of rotation
						xHit = xHit
							&& ((m_xRotNode->getScale().x * xIntersection.z) >= 0
							&& (m_xRotNode->getScale().y * xIntersection.y >= 0));
					}
				}

				if (zHit)
				{
					m_selectedAxis = axis_id::Z;
				}
				else if (yHit)
				{
					m_selectedAxis = axis_id::Y;
				}
				else if (xHit)
				{
					m_selectedAxis = axis_id::X;
				}
				else
				{
					m_selectedAxis = axis_id::None;
				}
			}
			else
			{
// 				if (!m_resetMouse)
// 				{
					Ogre::Vector3 axis(0.0f, 0.0f, 0.0f);
					Vector<float, 2> mouse(event->localPos().x() - m_lastMouse[0], event->localPos().y() - m_lastMouse[1]);
					Ogre::Radian angle(-mouse[0] * 0.008f);
					angle += Ogre::Radian(mouse[1] * 0.008f);
					switch (m_selectedAxis)
					{
						case axis_id::X:
						{
							axis = Ogre::Vector3::UNIT_X;
							if (m_camDir.x < 0)
							{
								angle *= -1.0f;
							}
							break;
						}

						case axis_id::Y:
						{
							axis = Ogre::Vector3::UNIT_Y;
							if (m_camDir.y < 0)
							{
								angle *= -1.0f;
							}
							break;
						}

						case axis_id::Z:
						{
							axis = Ogre::Vector3::UNIT_Z;
							if (m_camDir.z < 0)
							{
								angle *= -1.0f;
							}
							break;
						}
					}

					axis = m_widgetNode->getOrientation() * axis;
					Ogre::Quaternion rot(angle, axis);
					applyRotation(rot);

					//Cursor.Position = new Point(Cursor.Position.X - mouse.X, Cursor.Position.Y - mouse.Y);
					//m_resetMouse = true;
// 				}
// 				else
// 				{
// 					// This event was caused by resetting the cursor
// 					m_resetMouse = false;
// 				}
			}
		}

		void TransformWidget::scaleMouseMoved(Ogre::Ray &ray, const QMouseEvent *event)
		{
			if (!m_active)
			{
				m_selectedAxis = axis_id::None;

				// Inner verts
				Ogre::Vector3 innerY = m_relativeWidgetPos + m_widgetNode->getOrientation() * (Ogre::Vector3(0.0f, SquareLength, 0.0f) * m_widgetNode->getScale() * m_scaleNode->getScale());
				Ogre::Vector3 innerZ = m_relativeWidgetPos + m_widgetNode->getOrientation() * (Ogre::Vector3(0.0f, 0.0f, SquareLength) * m_widgetNode->getScale() * m_scaleNode->getScale());
				Ogre::Vector3 innerX = m_relativeWidgetPos + m_widgetNode->getOrientation() * (Ogre::Vector3(SquareLength, 0.0f, 0.0f) * m_widgetNode->getScale() * m_scaleNode->getScale());

				// Inner rectangle hit test for scale on all three axis
				auto res = Ogre::Math::intersects(ray, innerY, innerZ, innerX, true, true);
				if (res.first)
				{
					m_selectedAxis = AxisID(axis_id::All);
					return;
				}

				// Outer verts
				Ogre::Vector3 outerY = m_relativeWidgetPos + m_widgetNode->getOrientation() * (Ogre::Vector3(0.0f, LineLength * 0.75f, 0.0f) * m_widgetNode->getScale() * m_scaleNode->getScale());
				Ogre::Vector3 outerZ = m_relativeWidgetPos + m_widgetNode->getOrientation() * (Ogre::Vector3(0.0f, 0.0f, LineLength * 0.75f) * m_widgetNode->getScale() * m_scaleNode->getScale());
				Ogre::Vector3 outerX = m_relativeWidgetPos + m_widgetNode->getOrientation() * (Ogre::Vector3(LineLength * 0.75f, 0.0f, 0.0f) * m_widgetNode->getScale() * m_scaleNode->getScale());

				// XY check
				if (Ogre::Math::intersects(ray, outerX, innerX, innerY, true, true).first ||
					Ogre::Math::intersects(ray, innerY, outerY, outerX, true, true).first)
				{
					m_selectedAxis = AxisID(axis_id::XY);
					return;
				}

				// XZ check
				if (Ogre::Math::intersects(ray, outerX, innerX, innerZ, true, true).first ||
					Ogre::Math::intersects(ray, innerZ, outerZ, outerX, true, true).first)
				{
					m_selectedAxis = AxisID(axis_id::XZ);
					return;
				}

				// YZ check
				if (Ogre::Math::intersects(ray, outerY, innerY, innerZ, true, true).first ||
					Ogre::Math::intersects(ray, innerZ, outerZ, outerY, true, true).first)
				{
					m_selectedAxis = AxisID(axis_id::YZ);
					return;
				}

				if ((res = ray.intersects(getScalePlane(axis_id::X))).first)
				{
					Ogre::Vector3 dir = ray.getPoint(res.second) - m_relativeWidgetPos;
					dir = m_widgetNode->getOrientation().Inverse() * dir;

					if (dir.x > std::min(0.0f, LineLength * m_widgetNode->getScale().x * m_scaleNode->getScale().x) &&
						dir.x <= std::max(0.0f, LineLength * m_widgetNode->getScale().x * m_scaleNode->getScale().x))
					{
						Ogre::Vector3 projection = Ogre::Vector3::UNIT_X * dir.dotProduct(Ogre::Vector3::UNIT_X);
						Ogre::Vector3 difference = dir - projection;
						if (difference.length() < AxisBoxWidth * m_widgetNode->getScale().x)
						{
							m_selectedAxis = axis_id::X;
							return;
						}
					}
				}
				if ((res = ray.intersects(getTranslatePlane(axis_id::Y))).first)
				{
					Ogre::Vector3 dir = ray.getPoint(res.second) - m_relativeWidgetPos;
					dir = m_widgetNode->getOrientation().Inverse() * dir;

					if (dir.y > std::min(0.0f, LineLength * m_widgetNode->getScale().x * m_scaleNode->getScale().y) &&
						dir.y <= std::max(0.0f, LineLength * m_widgetNode->getScale().x * m_scaleNode->getScale().y))
					{
						Ogre::Vector3 projection = Ogre::Vector3::UNIT_Y * dir.dotProduct(Ogre::Vector3::UNIT_Y);
						Ogre::Vector3 difference = dir - projection;
						if (difference.length() < AxisBoxWidth * m_widgetNode->getScale().x)
						{
							m_selectedAxis = axis_id::Y;
							return;
						}
					}
				}
				if ((res = ray.intersects(getTranslatePlane(axis_id::Z))).first)
				{
					Ogre::Vector3 dir = ray.getPoint(res.second) - m_relativeWidgetPos;
					dir = m_widgetNode->getOrientation().Inverse() * dir;
					if (dir.z > std::min(0.0f, LineLength * m_widgetNode->getScale().x * m_scaleNode->getScale().z) &&
						dir.z <= std::max(0.0f, LineLength * m_widgetNode->getScale().x * m_scaleNode->getScale().z))
					{
						Ogre::Vector3 projection = Ogre::Vector3::UNIT_Z * dir.dotProduct(Ogre::Vector3::UNIT_Z);
						Ogre::Vector3 difference = dir - projection;
						if (difference.length() < AxisBoxWidth * m_widgetNode->getScale().x)
						{
							m_selectedAxis = axis_id::Z;
							return;
						}
					}
				}
			}
			else
			{
				// Translate
				if (!m_selection.empty())
				{
					Ogre::Vector3 distance(1.0f, 1.0f, 1.0f);

					const Ogre::Vector3 direction(
						((m_selectedAxis & axis_id::X) ? 1.0f : 0.0f),
						((m_selectedAxis & axis_id::Y) ? 1.0f : 0.0f),
						((m_selectedAxis & axis_id::Z) ? 1.0f : 0.0f));

					/*Ogre::Plane plane = getScalePlane(m_selectedAxis);
					auto res = ray.intersects(plane);
					if (res.first)
					{
						Ogre::Vector3 intersection = ray.getPoint(res.second);
						distance = intersection - m_lastIntersection;
						distance = m_widgetNode->getOrientation().Inverse() * distance;
						distance *= direction;
						distance = m_widgetNode->getOrientation() * distance;
						m_lastIntersection = intersection;
					}*/

					Vector<float, 2> mouse(event->localPos().x() - m_lastMouse[0], event->localPos().y() - m_lastMouse[1]);

					Ogre::Vector3 scale = distance + ((direction * (-mouse[1] * 0.008f)));
					applyScale(scale);
				}
			}
		}

		Ogre::Plane TransformWidget::getTranslatePlane(AxisID axis)
		{
			if (axis == (axis_id::X | axis_id::Z))
			{
				return Ogre::Plane(m_widgetNode->getOrientation() * Ogre::Vector3::UNIT_Y, m_relativeWidgetPos);
			}
			else if (axis == (axis_id::X | axis_id::Y))
			{
				return Ogre::Plane(m_widgetNode->getOrientation() * Ogre::Vector3::UNIT_Z, m_relativeWidgetPos);
			}
			else if (axis == (axis_id::Y | axis_id::Z))
			{
				return Ogre::Plane(m_widgetNode->getOrientation() * Ogre::Vector3::UNIT_X, m_relativeWidgetPos);
			}
			else
			{
				Ogre::Vector3 camDir = -m_relativeWidgetPos;
				camDir = m_widgetNode->getOrientation().Inverse() * camDir;

				switch (axis)
				{
					case axis_id::X:
					{
						camDir.x = 0.0f;
						break;
					}

					case axis_id::Y:
					{
						camDir.y = 0.0f;
						break;
					}

					case axis_id::Z:
					{
						camDir.z = 0.0f;
						break;
					}
				}

				camDir = m_widgetNode->getOrientation() * camDir;
				camDir.normalise();

				return Ogre::Plane(camDir, m_relativeWidgetPos);
			}
		}

		Ogre::Plane TransformWidget::getScalePlane(AxisID axis)
		{
			if (axis == (axis_id::X | axis_id::Z))
			{
				return Ogre::Plane(m_widgetNode->getOrientation() * Ogre::Vector3::UNIT_Y, m_relativeWidgetPos);
			}
			else if (axis == (axis_id::X | axis_id::Y))
			{
				return Ogre::Plane(m_widgetNode->getOrientation() * Ogre::Vector3::UNIT_Z, m_relativeWidgetPos);
			}
			else if (axis == (axis_id::Y | axis_id::Z))
			{
				return Ogre::Plane(m_widgetNode->getOrientation() * Ogre::Vector3::UNIT_X, m_relativeWidgetPos);
			}
			else
			{
				Ogre::Vector3 camDir = -m_relativeWidgetPos;
				camDir = m_widgetNode->getOrientation().Inverse() * camDir;

				switch (axis)
				{
					case axis_id::X:
					{
						camDir.x = 0.0f;
						break;
					}

					case axis_id::Y:
					{
						camDir.y = 0.0f;
						break;
					}

					case axis_id::Z:
					{
						camDir.z = 0.0f;
						break;
					}
				}

				camDir = m_widgetNode->getOrientation() * camDir;
				camDir.normalise();

				return Ogre::Plane(camDir, m_relativeWidgetPos);
			}
		}

		void TransformWidget::onSelectionChanged()
		{
			if (!m_selection.empty())
			{
				// Connect
				m_objectMovedCon = 
					m_selection.getSelectedObjects().back()->positionChanged.connect(this, &TransformWidget::onPositionChanged);

				// Get position
				const auto &pos = m_selection.getSelectedObjects().back()->getPosition();
				m_widgetNode->setPosition(pos[0], pos[1], pos[2]);
				m_relativeWidgetPos = m_widgetNode->getPosition() - m_camera.getRealPosition();

				if (m_isLocal)
				{
					auto rot = m_selection.getSelectedObjects().back()->getOrientation();
					m_widgetNode->setOrientation(Ogre::Quaternion(rot[0], rot[1], rot[2], rot[3]));
				}
				else
				{
					m_widgetNode->setOrientation(Ogre::Quaternion::IDENTITY);
				}

				setVisibility();
			}
			else
			{
				if (m_objectMovedCon.connected())
				{
					m_objectMovedCon.disconnect();
				}

				m_widgetNode->setVisible(false);
				m_selectedAxis = axis_id::None;
			}
		}

		void TransformWidget::onPositionChanged(const Selected &object)
		{
			const auto &pos = object.getPosition();

			Ogre::Vector3 ogrePos(pos[0], pos[1], pos[2]);
			m_widgetNode->setPosition(ogrePos);
			m_relativeWidgetPos = ogrePos - m_camera.getRealPosition();
		}

		void TransformWidget::onRotationChanged(const Selected &object)
		{

		}

		void TransformWidget::onScaleChanged(const Selected &object)
		{

		}

		void TransformWidget::cancelTransform()
		{

		}
	}
}
