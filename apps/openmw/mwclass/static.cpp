#include "static.hpp"

#include <components/esm3/loadstat.hpp>
#include <components/sceneutil/positionattitudetransform.hpp>

#include "../mwworld/ptr.hpp"
#include "../mwphysics/physicssystem.hpp"
#include "../mwworld/cellstore.hpp"

#include "../mwrender/objects.hpp"
#include "../mwrender/renderinginterface.hpp"
#include "../mwrender/vismask.hpp"

namespace MWClass
{
    Static::Static()
        : MWWorld::RegisteredClass<Static>(ESM::Static::sRecordId)
    {
    }

    void Static::insertObjectRendering (const MWWorld::Ptr& ptr, const std::string& model, MWRender::RenderingInterface& renderingInterface) const
    {
        if (!model.empty())
        {
            renderingInterface.getObjects().insertModel(ptr, model);
            ptr.getRefData().getBaseNode()->setNodeMask(MWRender::Mask_Static);
        }
    }

    void Static::insertObject(const MWWorld::Ptr& ptr, const std::string& model, const osg::Quat& rotation, MWPhysics::PhysicsSystem& physics) const
    {
        insertObjectPhysics(ptr, model, rotation, physics);
    }

    void Static::insertObjectPhysics(const MWWorld::Ptr& ptr, const std::string& model, const osg::Quat& rotation, MWPhysics::PhysicsSystem& physics) const
    {
        physics.addObject(ptr, model, rotation, MWPhysics::CollisionType_World);
    }

    std::string Static::getModel(const MWWorld::ConstPtr &ptr) const
    {
        const MWWorld::LiveCellRef<ESM::Static> *ref = ptr.get<ESM::Static>();

        const std::string &model = ref->mBase->mModel;
        if (!model.empty()) {
            return "meshes\\" + model;
        }
        return "";
    }

    std::string Static::getName (const MWWorld::ConstPtr& ptr) const
    {
        return "";
    }

    bool Static::hasToolTip(const MWWorld::ConstPtr& ptr) const
    {
        return false;
    }

    MWWorld::Ptr Static::copyToCellImpl(const MWWorld::ConstPtr &ptr, MWWorld::CellStore &cell) const
    {
        const MWWorld::LiveCellRef<ESM::Static> *ref = ptr.get<ESM::Static>();

        return MWWorld::Ptr(cell.insert(ref), &cell);
    }
}
