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

#pragma once

#include "common/typedefs.h"
#include "common/vector.h"
#include <QKeyEvent>
#include <QMouseEvent>

namespace wowpp
{
	class Selected;
	class Selection;

	namespace editor
	{
		namespace transform_mode
		{
			enum Type
			{
				Translate,
				Rotate,
				Scale
			};
		}

		typedef transform_mode::Type TransformMode;

		namespace axis_id
		{
			enum Type
			{
				None = 0,

				X = 1,
				Y = 2,
				Z = 4,

				XY = X | Y,
				XZ = X | Z,
				YZ = Y | Z,

				All = X | Y | Z
			};
		}

		typedef axis_id::Type AxisID;

		/// 
		class TransformWidget final
		{
		private:

			TransformWidget(const TransformWidget &Other) = delete;
			TransformWidget &operator=(const TransformWidget &Other) = delete;

		public:
			
			typedef simple::signal<void()> ModeChangeSignal;
			typedef simple::signal<void()> CoordinateSystemChangeSignal;

			ModeChangeSignal modeChanged;
			CoordinateSystemChangeSignal coordinateSystemChanged;

		public:

			explicit TransformWidget(
				Selection &selection, 
				Ogre::SceneManager &sceneMgr,
				Ogre::Camera &camera
				);
			~TransformWidget();

			/// @brief 
			/// 
			/// @param camera 
			/// 
			void update(Ogre::Camera *camera);
			/// @brief Should be called if a key was pressed.
			/// 
			/// @param event QKeyEvent which provides more informations about the event.
			/// 
			void onKeyPressed(const QKeyEvent *event);
			/// @brief Should be called if a key was released.
			/// 
			/// @param event QKeyEvent which provides more informations about the event.
			/// 
			void onKeyReleased(const QKeyEvent *event);
			/// @brief Should be called if a mouse button was pressed.
			/// 
			/// @param event QMouseEvent which provides more informations about the event.
			/// 
			void onMousePressed(const QMouseEvent *event);
			/// @brief Should be called if a mouse button was released.
			/// 
			/// @param event QMouseEvent which provides more informations about the event.
			/// 
			void onMouseReleased(const QMouseEvent *event);
			/// @brief Should be called if the mouse was moved.
			/// 
			/// @param event QMouseEvent which provides more informations about the event.
			/// 
			void onMouseMoved(const QMouseEvent *event);
			/// @brief Indicates whether the widget is active.
			/// 
			/// @returns True if the widget is active.
			/// 
			const bool isActive() const;
			/// @brief Sets the new transform mode of this widget.
			/// 
			/// @param mode The new transform mode.
			/// 
			void setTransformMode(TransformMode mode);
			/// @brief Sets whether the transform widget will be visible.
			/// 
			/// @param visible True if the new widget will be visible.
			/// 
			void setVisible(bool visible);

		private:

			/// @brief Initializes everything needed to display the translate widget.
			/// 
			void setupTranslation();
			/// @brief Initializes everything needed to display the rotate widget.
			/// 
			void setupRotation();
			/// @brief Initializes everything needed to display the scale widget.
			/// 
			void setupScale();
			/// @brief Updates the translation objects based on which axes are selected.
			/// 
			void updateTranslation();
			/// @brief Updates the rotation objects based on which axes are selected.
			/// 
			void updateRotation();
			/// @brief Updates the scale objects based on which axes are selected.
			/// 
			void updateScale();
			/// @brief 
			/// 
			void changeMode();
			/// @brief Updates the visibility of all transform widgets based on the mode.
			/// 
			void setVisibility();
			/// @brief 
			/// 
			/// @param dir
			/// 
			void applyTranslation(const Ogre::Vector3 &dir);
			/// @brief 
			/// 
			void finishTranslation();
			/// @brief 
			/// 
			/// @param rot 
			/// 
			void applyRotation(const Ogre::Quaternion &rot);
			/// @brief 
			/// 
			void finishRotation();
			/// @brief 
			/// 
			/// @param dir 
			/// 
			void applyScale(const Ogre::Vector3 &dir);
			/// @brief 
			/// 
			void finishScale();
			/// @brief 
			/// 
			/// @param ray
			/// 
			void translationMouseMoved(Ogre::Ray &ray, const QMouseEvent *event);
			/// @brief 
			/// 
			/// @param ray
			/// 
			void rotationMouseMoved(Ogre::Ray &ray, const QMouseEvent *event);
			/// @brief 
			/// 
			/// @param ray 
			/// 
			void scaleMouseMoved(Ogre::Ray &ray, const QMouseEvent *event);
			/// @brief 
			/// 
			/// @param axis 
			/// 
			/// @returns 
			/// 
			Ogre::Plane getTranslatePlane(AxisID axis);
			/// @brief 
			/// 
			/// @param axis 
			/// 
			/// @returns 
			/// 
			Ogre::Plane getScalePlane(AxisID axis);
			/// @brief
			/// 
			void onSelectionChanged();
			/// @brief 
			/// 
			/// @param object
			/// 
			void onPositionChanged(const Selected &object);
			/// @brief 
			/// 
			/// @param object
			/// 
			void onRotationChanged(const Selected &object);
			/// @brief 
			/// 
			/// @param object
			/// 
			void onScaleChanged(const Selected &object);
			/// @brief 
			/// 
			void cancelTransform();

		private:

			static const float LineLength;
			static const float CenterOffset;
			static const float SquareLength;
			static const float AxisBoxWidth;
			static const float PlaneHeight;
			static const float TipLength;
			static const float OrthoScale;
			// Translation
			static const Ogre::ColourValue ColorXAxis;
			static const Ogre::ColourValue ColorYAxis;
			static const Ogre::ColourValue ColorZAxis;
			static const Ogre::ColourValue ColorSelectedAxis;
			static const Ogre::String AxisPlaneName;
			static const Ogre::String ArrowMeshName;
			// Rotation
			static const float OuterRadius;
			static const float InnerRadius;
			static const Ogre::String CircleMeshName;
			static const Ogre::String FullCircleMeshName;
			// Scale
			static const Ogre::String ScaleAxisPlaneName;
			static const Ogre::String ScaleContentPlaneName;

		private:

			TransformMode m_mode;
			bool m_isLocal;
			float m_step;
			AxisID m_selectedAxis;
			Selection &m_selection;
			Ogre::Vector3 m_snappedTranslation;
			Ogre::Camera *m_dummyCamera;
			Ogre::Vector3 m_relativeWidgetPos;
			Ogre::SceneNode *m_widgetNode;
			Ogre::SceneNode *m_translationNode;
			Ogre::SceneNode *m_rotationNode;
			Ogre::SceneNode *m_scaleNode;
			Ogre::SceneManager &m_sceneMgr;
			Ogre::Camera &m_camera;
			bool m_active;
			Vector<size_t, 2> m_lastMouse;
			Ogre::Vector3 m_lastIntersection;
			Ogre::Vector3 m_translation;
			Ogre::Quaternion m_rotation;
			float m_scale;
			bool m_keyDown;
			bool m_resetMouse;
			bool m_copyMode;
			bool m_sleep;
			Ogre::Vector3 m_camDir;
			bool m_visible;
			simple::scoped_connection m_objectMovedCon, m_onSelectionChanged;

			// Translation-Mode variables
			Ogre::ManualObject *m_axisLines;
			Ogre::SceneNode *m_xArrowNode;
			Ogre::SceneNode *m_yArrowNode;
			Ogre::SceneNode *m_zArrowNode;
			Ogre::Entity *m_xArrow;
			Ogre::Entity *m_yArrow;
			Ogre::Entity *m_zArrow;
			Ogre::SceneNode *m_xzPlaneNode;
			Ogre::SceneNode *m_xyPlaneNode;
			Ogre::SceneNode *m_yzPlaneNode;
			Ogre::MeshPtr m_translateAxisPlanes;

			// Rotation-Mode variables
			Ogre::SceneNode *m_zRotNode;
			Ogre::SceneNode *m_xRotNode;
			Ogre::SceneNode *m_yRotNode;
			Ogre::Entity *m_xCircle;
			Ogre::Entity *m_yCircle;
			Ogre::Entity *m_zCircle;
			Ogre::Entity *m_fullCircleEntity;
			Ogre::SceneNode *m_rotationCenter;
			Ogre::MeshPtr m_circleMesh;
			Ogre::MeshPtr m_fullCircleMesh;

			// Scale-Mode variables
			Ogre::ManualObject *m_scaleAxisLines;
			Ogre::SceneNode *m_scaleXZPlaneNode;
			Ogre::SceneNode *m_scaleXYPlaneNode;
			Ogre::SceneNode *m_scaleYZPlaneNode;
			Ogre::SceneNode *m_scaleContentPlaneNode;
			Ogre::MeshPtr m_scaleAxisPlanes;
			Ogre::MeshPtr m_scaleCenterPlanes;
		};
	}
}

