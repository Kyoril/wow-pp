
#include "world_editor.h"
#include "data/map_entry.h"
#include "data/project.h"
#include "common/typedefs.h"
#include "common/constants.h"
#include "common/make_unique.h"
#include "game/constants.h"
#include "log/default_log_levels.h"
#include "OgreCamera.h"
#include "OgreLight.h"
#include "OgreSceneManager.h"
#include "OgreSceneNode.h"
#include "OgreMesh.h"
#include "OgreSubMesh.h"
#include "OgreResourceManager.h"
#include "OgreException.h"
#include "OgreDataStream.h"

namespace wowpp
{
	namespace editor
	{
		struct M2Header
		{
			UInt32 magic;
			UInt32 version;
			UInt32 nameLen;
			UInt32 nameOffset;
			UInt32 globalFlags;
			UInt32 nGlobalSequences;
			UInt32 ofsGlobalSequences;
			UInt32 nAnimations;
			UInt32 ofsAnimations;
			UInt32 nAnimationLookup;
			UInt32 ofsAnimationLookup;
			UInt32 nPlayableAnimationLookup;
			UInt32 ofsPlayableAnimationLookup;
			UInt32 nBones;
			UInt32 ofsBones;
			UInt32 nSkelBoneLookup;
			UInt32 ofsSkelBoneLookup;
			UInt32 nVertices;
			UInt32 ofsVertices;
			UInt32 nViews;
			UInt32 ofsViews;
			UInt32 nColors;
			UInt32 ofsColors;
			UInt32 nTextures;
			UInt32 ofsTextures;
			UInt32 nTransparency;
			UInt32 ofsTransparency;
			UInt32 nI;
			UInt32 ofsI;
			UInt32 nTexAnims;
			UInt32 ofsTexAnims;
			UInt32 nTexReplace;
			UInt32 ofsTexReplace;
			UInt32 nRenderFlags;
			UInt32 ofsRenderFlags;
			UInt32 nBoneLookupTable;
			UInt32 ofsBoneLookupTable;
			UInt32 nTexLookup;
			UInt32 ofsTexLookup;
			UInt32 nTexUnits;
			UInt32 ofsTexUnits;
			UInt32 nTransLookup;
			UInt32 ofsTransLookup;
			UInt32 nTexAnimLookup;
			UInt32 ofsTexAnimLookup;
			std::array<float, 14> values;
			UInt32 nBoundingTriangles;
			UInt32 ofsBoundingTriangles;
			UInt32 nBoundingVertices;
			UInt32 ofsBoundingVertices;
			UInt32 nBoundingNormals;
			UInt32 ofsBoundingNormals;
			UInt32 nAttachments;
			UInt32 ofsAttachments;
			UInt32 nAttachLookup;
			UInt32 ofsAttachLookup;
			UInt32 nAttachments_2;
			UInt32 ofsAttachments_2;
			UInt32 nLights;
			UInt32 ofsLights;
			UInt32 nCameras;
			UInt32 ofsCameras;
			UInt32 nCameraLookup;
			UInt32 ofsCameraLookup;
			UInt32 nRibbonEmitters;
			UInt32 ofsRibbonEmitters;
			UInt32 nParticleEmitters;
			UInt32 ofsParticleEmitters;
		};

		struct M2View
		{
			UInt32 nIndices;
			UInt32 ofsIndices;
			UInt32 nTriangles;
			UInt32 ofsTriangles;
			UInt32 nVertexProperties;
			UInt32 ofsVertexProperties;
			UInt32 nSubMeshs;
			UInt32 ofsSubMeshs;
			UInt32 nTextures;
			UInt32 ofsTextures;
		};

		struct M2SubMesh
		{
			UInt32 id;
			UInt16 startVert;
			UInt16 nVerts;
			UInt16 startTri;
			UInt16 nTris;
			UInt16 nBones;
			UInt16 startBone;
			UInt16 unkBone1;
			UInt16 rootBone;
			float vec1[3];
			float vec2[4];
		};

		typedef UInt16 M2Index;

		struct M2Texture
		{
			UInt32 type;
			UInt16 unk1;
			UInt16 flags;
			UInt32 fileNameLen;
			UInt32 fileNameOffs;
		};

		class M2MeshLoader : public Ogre::ManualResourceLoader
		{
		private:

			Ogre::String m_fileName;

		public:

			M2MeshLoader(Ogre::String fileName)
				: m_fileName(std::move(fileName))
			{
			}

			virtual void loadResource(Ogre::Resource* resource) override
			{
				// Load mesh resource
				Ogre::Mesh* mesh = dynamic_cast<Ogre::Mesh*>(resource);
				if (!mesh)
				{
					OGRE_EXCEPT(Ogre::Exception::ERR_INVALID_CALL, "M2MeshLoader can only load Ogre::Mesh resource objects", "M2MeshLoader::loadResource");
				}

				Ogre::DataStreamPtr ptr = Ogre::ResourceGroupManager::getSingleton().openResource(m_fileName, "WoW", false);
				if (!ptr.isNull())
				{
					M2Header header;
					ptr->read(&header, sizeof(M2Header));
					if (header.magic != 0x3032444D)
					{
						OGRE_EXCEPT(Ogre::Exception::ERR_INVALID_STATE, "M2MeshLoader: Invalid m2 mesh header", "M2MeshLoader::loadResource");
					}

					// Read model name
					Ogre::String modelName(header.nameLen - 1, '\0');
					ptr->seek(header.nameOffset);
					ptr->readLine(&modelName[0], header.nameLen - 1, "\0");

					// Create and assign vertex data object
					Ogre::VertexData *vertData = new Ogre::VertexData();
					mesh->sharedVertexData = vertData;
					vertData->vertexStart = 0;
					vertData->vertexCount = header.nVertices;

					// Read texture definitions
					std::vector<M2Texture> textures(header.nTextures);
					if (header.ofsTextures && header.nTextures)
					{
						ptr->seek(header.ofsTextures);
						ptr->read(&textures[0], sizeof(M2Texture) * header.nTextures);
					}

					// Setup vertex attribute declaration
					Ogre::VertexDeclaration* decl = vertData->vertexDeclaration;
					size_t offset = decl->addElement(0, 0, Ogre::VET_FLOAT3, Ogre::VES_POSITION).getSize();
					offset += decl->addElement(0, offset, Ogre::VET_FLOAT3, Ogre::VES_NORMAL).getSize();
					offset += decl->addElement(0, offset, Ogre::VET_FLOAT2, Ogre::VES_TEXTURE_COORDINATES, 0).getSize();

					// Create the vertex buffer and write vertex data
					Ogre::HardwareVertexBufferSharedPtr vbuf = Ogre::HardwareBufferManager::getSingleton().createVertexBuffer(
						decl->getVertexSize(0),
						header.nVertices,
						Ogre::HardwareBuffer::HBU_STATIC_WRITE_ONLY
						);

					// Bind the newly created vertex buffer
					Ogre::VertexBufferBinding* bind = vertData->vertexBufferBinding;
					bind->setBinding(0, vbuf);

					Ogre::Vector3 vMin(99999.0f, 99999.0f, 99999.0f);
					Ogre::Vector3 vMax(-99999.0f, -99999.0f, -99999.0f);

					std::vector<float> verts(8 * header.nVertices, 0.0f);
					float *base = &verts[0];

					// Skip to the specified position
					ptr->seek(header.ofsVertices);
					for (UInt32 i = 0; i < header.nVertices; ++i)
					{
						// Read position data
						ptr->read(base, sizeof(float) * 3);
						if (*base < vMin.x) vMin.x = *base;
						if (*(base + 1) < vMin.y) vMin.y = *(base + 1);
						if (*(base + 2) < vMin.z) vMin.z = *(base + 2);
						if (*base > vMax.x) vMax.x = *base;
						if (*(base + 1) > vMax.y) vMax.y = *(base + 1);
						if (*(base + 2) > vMax.z) vMax.z = *(base + 2);
						base += 3;

						// Skip bone data
						ptr->skip(sizeof(unsigned char) * 8);

						// Read normal data
						ptr->read(base, sizeof(float) * 3);
						base += 3;

						// Read uv data and invert v
						ptr->read(base, sizeof(float) * 2);
						*(base + 1) = 1.0f - *(base + 1);
						base += 2;

						// Skip unknown data
						ptr->skip(sizeof(float) * 2);
					}

					vbuf->writeData(0, verts.size() * sizeof(float), &verts[0], true);

					// Read views
					if (header.nViews > 0)
					{
						// Seek view offset
						ptr->seek(header.ofsViews);

						// Read first view
						M2View view;
						ptr->read(&view, sizeof(M2View));

						// Read index list
						std::vector<M2Index> indices(view.nIndices, 0);
						ptr->seek(view.ofsIndices);
						ptr->read(&indices[0], sizeof(M2Index) * view.nIndices);

						// Read triangle list
						std::vector<M2Index> triangles(view.nTriangles);
						ptr->seek(view.ofsTriangles);
						ptr->read(&triangles[0], sizeof(M2Index) * view.nTriangles);

						// Create the index buffer and fill it with data
						Ogre::HardwareIndexBufferSharedPtr ibuf = Ogre::HardwareBufferManager::getSingleton().createIndexBuffer(
							Ogre::HardwareIndexBuffer::IT_16BIT,
							view.nTriangles,
							Ogre::HardwareBuffer::HBU_DYNAMIC_WRITE_ONLY
							);
						unsigned short* base = static_cast<unsigned short*>(ibuf->lock(0, ibuf->getSizeInBytes(), Ogre::HardwareBuffer::HBL_NORMAL));
						{
							for (UInt16 j = 0; j < view.nTriangles; ++j)
							{
								M2Index ind = indices[triangles[j]];
								*base++ = ind;
							}
						}
						ibuf->unlock();

						// Load all submeshs
						ptr->seek(view.ofsSubMeshs);
						for (UInt32 i = 0; i < view.nSubMeshs; ++i)
						{
							// Load submesh properties
							M2SubMesh sub;
							ptr->read(&sub, sizeof(M2SubMesh));

							const Ogre::String materialName = modelName + "_" + Ogre::StringConverter::toString(i);
							Ogre::MaterialPtr mat = Ogre::MaterialManager::getSingleton().create(materialName, "WoW");
							mat->removeAllTechniques();

							auto *teq = mat->createTechnique();
							teq->removeAllPasses();

							auto *pass = teq->createPass();
							pass->removeAllTextureUnitStates();
							pass->setSceneBlending(Ogre::SBF_ONE, Ogre::SBF_ONE_MINUS_DEST_ALPHA);

							auto *texUnit = pass->createTextureUnitState();
							texUnit->setTextureName("CREATURE\\Kobold\\koboldskinNormal.blp");

							// Create a new submesh
							Ogre::SubMesh *subMesh = mesh->createSubMesh();
							subMesh->useSharedVertices = true;
							subMesh->indexData = new Ogre::IndexData();
							subMesh->indexData->indexStart = sub.startTri;
							subMesh->indexData->indexCount = sub.nTris;
							subMesh->indexData->indexBuffer = ibuf;
							subMesh->setMaterialName(materialName);
						}
					}

					mesh->_setBounds(Ogre::AxisAlignedBox(vMin, vMax));
					mesh->_setBoundingSphereRadius(std::max(vMax.x - vMin.x, std::max(vMax.y - vMin.y, vMax.z - vMin.z)) / 2.0f);
				}
			}
		};

		WorldEditor::WorldEditor(Ogre::SceneManager &sceneMgr, Ogre::Camera &camera, MapEntry &map)
			: m_sceneMgr(sceneMgr)
			, m_camera(camera)
			, m_map(map)
			, m_work(new boost::asio::io_service::work(m_workQueue))
            , m_light(nullptr)
		{
			// Create worker thread
			boost::asio::io_service &workQueue = m_workQueue;
			boost::asio::io_service &dispatcher = m_dispatcher;
			m_worker = boost::thread([&workQueue]()
			{
				workQueue.run();
			});

			// Move camera to first spawn position (if any)
			if (!m_map.spawns.empty())
			{
				const auto &spawn = m_map.spawns.front();
				m_camera.setPosition(spawn.position[0], spawn.position[1], spawn.position[2] + 5.0f);
			}

			Ogre::MeshPtr mesh = Ogre::MeshManager::getSingleton().createManual("Kobold", "WoW", new M2MeshLoader("CREATURE\\Kobold\\kobold.m2"));
			mesh->load();

			// Spawn all entities
			UInt32 i = 0;
			for (auto &spawn : m_map.spawns)
			{
				Ogre::Entity *ent = m_sceneMgr.createEntity("Spawn_" + Ogre::StringConverter::toString(i++), mesh);
				Ogre::SceneNode *node = m_sceneMgr.getRootSceneNode()->createChildSceneNode();
				node->attachObject(ent);
				node->setPosition(spawn.position[0], spawn.position[1], spawn.position[2]);
				node->setOrientation(Ogre::Quaternion(Ogre::Radian(spawn.rotation), Ogre::Vector3::UNIT_Z));
			}

			const Ogre::Vector3 &camPos = m_camera.getDerivedPosition();
			float convertedX = (constants::MapWidth * 32.0f) + camPos.x;
			float convertedY = (constants::MapWidth * 32.0f) + camPos.y;

			paging::PagePosition pos(63 - static_cast<size_t>(convertedX / constants::MapWidth), 63 - static_cast<size_t>(convertedY / constants::MapWidth));
			const auto addWork = [&workQueue](const WorldPageLoader::Work &work)
			{
                workQueue.post(work);
			};

			const auto synchronize = [&dispatcher](const WorldPageLoader::Work &work)
			{
				dispatcher.post(work);
			};

			m_visibleSection = make_unique<paging::LoadedPageSection>(pos, 1, *this);
			m_pageLoader = make_unique<WorldPageLoader>(
				*m_visibleSection.get(),
				addWork,
				synchronize
				);

			paging::PagePosition worldSize(constants::PagesPerMap, constants::PagesPerMap);
			m_memoryPointOfView = make_unique<paging::PagePOVPartitioner>(
				worldSize,
				1,
				pos,
				*m_pageLoader.get());

			// Create the world renderer
			m_worldRenderer.reset(new view::WorldRenderer(
				m_sceneMgr,
				m_camera));
            
			m_light = m_sceneMgr.createLight("Sun");
            m_light->setType(Ogre::Light::LT_DIRECTIONAL);
            m_light->setDirection(Ogre::Vector3(0.0f, 0.5f, -0.5f).normalisedCopy());
			m_sceneMgr.getRootSceneNode()->attachObject(m_light);
		}

		WorldEditor::~WorldEditor()
		{
            if (m_light)
            {
				m_sceneMgr.destroyLight(m_light);
                m_light = nullptr;
            }
            
			// Stop paging
			m_work.reset();
			m_workQueue.stop();
			m_worker.join();
		}

		void WorldEditor::update(float delta)
		{
			const Ogre::Vector3 &camPos = m_camera.getDerivedPosition();
			float convertedX = (constants::MapWidth * 32.0f) + camPos.x;
			float convertedY = (constants::MapWidth * 32.0f) + camPos.y;

			paging::PagePosition pos(63 - static_cast<size_t>(convertedX / constants::MapWidth), 63 - static_cast<size_t>(convertedY / constants::MapWidth));

			m_memoryPointOfView->updateCenter(pos);
			m_visibleSection->updateCenter(pos);

			// Update paging
			m_dispatcher.poll();

			m_worldRenderer->update(delta);
		}

		void WorldEditor::save()
		{
			//TODO
		}

		void WorldEditor::onPageLoad(const paging::Page &page)
		{
			const auto &pos = page.getPosition();

			// Add the page if needed
			auto it = m_pages.find(pos);
			if (it == m_pages.end())
			{
				const Ogre::String fileName =
					"World\\Maps\\" + m_map.directory + "\\" + m_map.directory + "_" +
					Ogre::StringConverter::toString(pos[1], 2, '0') + "_" +
					Ogre::StringConverter::toString(pos[0], 2, '0') + ".adt";

				if (!Ogre::ResourceGroupManager::getSingleton().resourceExistsInAnyGroup(fileName))
				{
					return;
				}

				Ogre::DataStreamPtr file = Ogre::ResourceGroupManager::getSingleton().openResource(fileName);
				std::unique_ptr<adt::Page> page = make_unique<adt::Page>();
				adt::load(file, *page);

				// Store page
				m_pages[pos] = std::move(page->terrain);
			}
		}

		void WorldEditor::onPageAvailabilityChanged(const paging::PageNeighborhood &pages, bool isAvailable)
		{
			auto &mainPage = pages.getMainPage();
			const auto &pos = mainPage.getPosition();

			if (isAvailable)
			{
				// Add the page if needed
				auto it = m_pages.find(pos);
				if (it != m_pages.end())
				{
					// Add terrain page
					terrain::editing::PageAdded add;
					add.added.page = &m_pages[pos];
					add.added.position = pos;
					m_worldRenderer->handleEvent(terrain::editing::TerrainChangeEvent(add));
				}
			}
			else
			{
				// Add the page if needed
				auto it = m_pages.find(pos);
				if (it != m_pages.end())
				{
					// Remove terrain page
					terrain::editing::PageRemoved remove;
					remove.removed = pos;
					m_worldRenderer->handleEvent(terrain::editing::TerrainChangeEvent(remove));

					// Remove page from list
					m_pages.erase(it);
				}
			}
		}

		terrain::model::Page * WorldEditor::getTerrainPage(terrain::model::PagePosition position)
		{
			// Add the page if needed
			auto it = m_pages.find(position);
			if (it != m_pages.end())
			{
				return &it->second;
			}

			return nullptr;
		}

		void WorldEditor::onKeyPressed(const QKeyEvent *event)
		{
		}

		void WorldEditor::onKeyReleased(const QKeyEvent *event)
		{
		}

		void WorldEditor::onMousePressed(const QMouseEvent *event)
		{
		}

		void WorldEditor::onMouseReleased(const QMouseEvent *event)
		{
		}

		void WorldEditor::onMouseMoved(const QMouseEvent *event)
		{
		}
	}
}
