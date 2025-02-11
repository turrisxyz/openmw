#ifndef OPENMW_COMPONENTS_DETOURNAVIGATOR_NAVMESHMANAGER_H
#define OPENMW_COMPONENTS_DETOURNAVIGATOR_NAVMESHMANAGER_H

#include "asyncnavmeshupdater.hpp"
#include "cachedrecastmeshmanager.hpp"
#include "offmeshconnectionsmanager.hpp"
#include "recastmeshtiles.hpp"
#include "waitconditiontype.hpp"
#include "heightfieldshape.hpp"

#include <BulletCollision/CollisionShapes/btHeightfieldTerrainShape.h>

#include <osg/Vec3f>

#include <map>
#include <memory>

class dtNavMesh;

namespace DetourNavigator
{
    class NavMeshManager
    {
    public:
        explicit NavMeshManager(const Settings& settings, std::unique_ptr<NavMeshDb>&& db);

        void setWorldspace(std::string_view worldspace);

        void updateBounds(const osg::Vec3f& playerPosition);

        bool addObject(const ObjectId id, const CollisionShape& shape, const btTransform& transform,
                       const AreaType areaType);

        bool updateObject(const ObjectId id, const CollisionShape& shape, const btTransform& transform,
                          const AreaType areaType);

        bool removeObject(const ObjectId id);

        void addAgent(const osg::Vec3f& agentHalfExtents);

        bool addWater(const osg::Vec2i& cellPosition, int cellSize, float level);

        bool removeWater(const osg::Vec2i& cellPosition);

        bool addHeightfield(const osg::Vec2i& cellPosition, int cellSize, const HeightfieldShape& shape);

        bool removeHeightfield(const osg::Vec2i& cellPosition);

        bool reset(const osg::Vec3f& agentHalfExtents);

        void addOffMeshConnection(const ObjectId id, const osg::Vec3f& start, const osg::Vec3f& end, const AreaType areaType);

        void removeOffMeshConnections(const ObjectId id);

        void update(const osg::Vec3f& playerPosition, const osg::Vec3f& agentHalfExtents);

        void wait(Loading::Listener& listener, WaitConditionType waitConditionType);

        SharedNavMeshCacheItem getNavMesh(const osg::Vec3f& agentHalfExtents) const;

        std::map<osg::Vec3f, SharedNavMeshCacheItem> getNavMeshes() const;

        void reportStats(unsigned int frameNumber, osg::Stats& stats) const;

        RecastMeshTiles getRecastMeshTiles() const;

    private:
        const Settings& mSettings;
        std::string mWorldspace;
        TileCachedRecastMeshManager mRecastMeshManager;
        OffMeshConnectionsManager mOffMeshConnectionsManager;
        AsyncNavMeshUpdater mAsyncNavMeshUpdater;
        std::map<osg::Vec3f, SharedNavMeshCacheItem> mCache;
        std::map<osg::Vec3f, std::map<TilePosition, ChangeType>> mChangedTiles;
        std::size_t mGenerationCounter = 0;
        std::map<osg::Vec3f, TilePosition> mPlayerTile;
        std::map<osg::Vec3f, std::size_t> mLastRecastMeshManagerRevision;

        void addChangedTiles(const btCollisionShape& shape, const btTransform& transform, const ChangeType changeType);

        void addChangedTiles(const int cellSize, const btVector3& shift, const ChangeType changeType);

        void addChangedTile(const TilePosition& tilePosition, const ChangeType changeType);

        SharedNavMeshCacheItem getCached(const osg::Vec3f& agentHalfExtents) const;
    };
}

#endif
