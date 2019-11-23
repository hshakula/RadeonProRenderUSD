#ifndef HDRPR_RPR_API_MATERIAL_H
#define HDRPR_RPR_API_MATERIAL_H

#include "pxr/pxr.h"

#include <RadeonProRender.h>
#include <memory>
#include <vector>

PXR_NAMESPACE_OPEN_SCOPE

class HdRprApi;
class ImageCache;
class RprApiObject;
class MaterialAdapter;

namespace rpr {

class Image;

} // namespace rpr

/// \class HdRprApiMaterial
///
/// In hdRpr material creation made in a lazy way:
///   1. The initial state of a material is set by MaterialAdapter
///   2. HdRprApi commits the initial state to get actual RPR material
class HdRprApiMaterial {
public:
    HdRprApiMaterial(std::unique_ptr<MaterialAdapter>&& adapter);

    void AttachToShape(RprApiObject* shape);
    void AttachToCurve(RprApiObject* curve);

private:
    class State {
    public:
        virtual ~State() = default;

        virtual void AttachToShape(RprApiObject* shape) = 0;
        virtual void AttachToCurve(RprApiObject* curve) = 0;
    };

    class InitialState : public State {
    public:
        InitialState(std::unique_ptr<MaterialAdapter>&& adapter);
        ~InitialState() override;

        void AttachToShape(RprApiObject* shape) override;
        void AttachToCurve(RprApiObject* shape) override;

        MaterialAdapter const& GetMaterialAdapter() const { return *m_adapter; }
        std::vector<RprApiObject*> const& GetAttachedShapes() const { return m_attachedShapes; };
        std::vector<RprApiObject*> const& GetAttachedCurves() const { return m_attachedCurves; };

    private:
        std::unique_ptr<MaterialAdapter> m_adapter;
        std::vector<RprApiObject*> m_attachedShapes;
        std::vector<RprApiObject*> m_attachedCurves;
    };

    class CompiledState : public State {
    public:
        static std::unique_ptr<CompiledState> Create(InitialState const* state,
                                                     rpr_material_system matSys,
                                                     ImageCache* imageCache);
        ~CompiledState() override;

        void AttachToShape(RprApiObject* shape) override;
        void AttachToCurve(RprApiObject* shape) override;

    private:
        CompiledState() = default;

    private:
        rpr_material_node m_rootMaterial;
        rpr_material_node m_displacementMaterial;
        std::vector<std::shared_ptr<rpr::Image>> m_materialImages;
        std::vector<rpr_material_node> m_materialNodes;

        ImageCache* m_imageCache;
        std::vector<RprApiObject*> m_attachedShapes;
        std::vector<RprApiObject*> m_attachedCurves;
    };

    friend class HdRprApiImpl;

private:
    std::unique_ptr<State> m_state;
};

PXR_NAMESPACE_CLOSE_SCOPE

#endif // HDRPR_RPR_API_MATERIAL_H
