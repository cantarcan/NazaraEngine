// Copyright (C) 2013 Jérôme Leclercq
// This file is part of the "Nazara Engine - Graphics module"
// For conditions of distribution and use, see copyright notice in Config.hpp

#pragma once

#ifndef NAZARA_MODEL_HPP
#define NAZARA_MODEL_HPP

#include <Nazara/Prerequesites.hpp>
#include <Nazara/Core/ResourceLoader.hpp>
#include <Nazara/Core/Updatable.hpp>
#include <Nazara/Graphics/SceneNode.hpp>
#include <Nazara/Renderer/Material.hpp>
#include <Nazara/Utility/Animation.hpp>
#include <Nazara/Utility/Mesh.hpp>

struct NAZARA_API NzModelParameters
{
	bool loadAnimation = true;
	bool loadMaterials = true;
	NzAnimationParams animation;
	NzMaterialParams material;
	NzMeshParams mesh;

	bool IsValid() const;
};

class NzModel;

using NzModelLoader = NzResourceLoader<NzModel, NzModelParameters>;

class NAZARA_API NzModel : public NzSceneNode, public NzUpdatable
{
	friend NzModelLoader;
	friend class NzScene;

	public:
		NzModel();
		NzModel(const NzModel& model);
		NzModel(NzModel&& model);
		~NzModel();

		void AddToRenderQueue(NzRenderQueue& renderQueue) const;
		void AdvanceAnimation(float elapsedTime);

		void EnableAnimation(bool animation);
		void EnableDraw(bool draw);

		NzAnimation* GetAnimation() const;
		const NzBoundingBoxf& GetBoundingBox() const;
		NzMaterial* GetMaterial(unsigned int matIndex) const;
		NzMaterial* GetMaterial(unsigned int skinIndex, unsigned int matIndex) const;
		unsigned int GetMaterialCount() const;
		unsigned int GetSkin() const;
		unsigned int GetSkinCount() const;
		NzMesh* GetMesh() const;
		nzSceneNodeType GetSceneNodeType() const override;
		NzSkeleton* GetSkeleton();
		const NzSkeleton* GetSkeleton() const;

		bool HasAnimation() const;

		bool IsAnimationEnabled() const;
		bool IsDrawEnabled() const;

		bool LoadFromFile(const NzString& filePath, const NzModelParameters& params = NzModelParameters());
		bool LoadFromMemory(const void* data, std::size_t size, const NzModelParameters& params = NzModelParameters());
		bool LoadFromStream(NzInputStream& stream, const NzModelParameters& params = NzModelParameters());

		void Reset();

		bool SetAnimation(NzAnimation* animation);
		void SetMaterial(unsigned int matIndex, NzMaterial* material);
		void SetMaterial(unsigned int skinIndex, unsigned int matIndex, NzMaterial* material);
		void SetMesh(NzMesh* mesh);
		bool SetSequence(const NzString& sequenceName);
		void SetSequence(unsigned int sequenceIndex);
		void SetSkin(unsigned int skin);
		void SetSkinCount(unsigned int skinCount);

		NzModel& operator=(const NzModel& node);
		NzModel& operator=(NzModel&& node);

	private:
		void Invalidate() override;
		void Register() override;
		void Unregister() override;
		void Update() override;
		void UpdateBoundingBox() const;
		bool VisibilityTest(const NzFrustumf& frustum) override;

		std::vector<NzMaterialRef> m_materials;
		mutable NzBoundingBoxf m_boundingBox;
		NzSkeleton m_skeleton; // Uniquement pour les animations squelettiques
		NzAnimationRef m_animation;
		NzMeshRef m_mesh;
		const NzSequence* m_currentSequence;
		bool m_animationEnabled;
		mutable bool m_boundingBoxUpdated;
		bool m_drawEnabled;
		float m_interpolation;
		unsigned int m_currentFrame;
		unsigned int m_matCount;
		unsigned int m_nextFrame;
		unsigned int m_skin;
		unsigned int m_skinCount;

		static NzModelLoader::LoaderList s_loaders;
};

#endif // NAZARA_MODEL_HPP