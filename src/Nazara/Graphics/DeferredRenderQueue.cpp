// Copyright (C) 2013 Jérôme Leclercq
// This file is part of the "Nazara Engine - Graphics module"
// For conditions of distribution and use, see copyright notice in Config.hpp

#include <Nazara/Graphics/DeferredRenderQueue.hpp>
#include <Nazara/Graphics/Camera.hpp>
#include <Nazara/Graphics/ForwardRenderQueue.hpp>
#include <Nazara/Graphics/Light.hpp>
#include <Nazara/Graphics/Model.hpp>
#include <Nazara/Graphics/Sprite.hpp>
#include <Nazara/Renderer/Material.hpp>
#include <Nazara/Utility/SkeletalMesh.hpp>
#include <Nazara/Utility/StaticMesh.hpp>
#include <Nazara/Graphics/Debug.hpp>

namespace
{
	enum ResourceType
	{
		ResourceType_Material,
		ResourceType_SkeletalMesh,
		ResourceType_StaticMesh
	};
}

NzDeferredRenderQueue::NzDeferredRenderQueue(NzForwardRenderQueue* forwardQueue) :
m_forwardQueue(forwardQueue)
{
}

NzDeferredRenderQueue::~NzDeferredRenderQueue()
{
	Clear(true);
}

void NzDeferredRenderQueue::AddDrawable(const NzDrawable* drawable)
{
	m_forwardQueue->AddDrawable(drawable);
}

void NzDeferredRenderQueue::AddLight(const NzLight* light)
{
	#if NAZARA_GRAPHICS_SAFE
	if (!light)
	{
		NazaraError("Invalid light");
		return;
	}
	#endif

	switch (light->GetLightType())
	{
		case nzLightType_Directional:
			directionalLights.push_back(light);
			break;

		case nzLightType_Point:
			pointLights.push_back(light);
			break;

		case nzLightType_Spot:
			spotLights.push_back(light);
			break;
	}

	m_forwardQueue->AddLight(light);
}

void NzDeferredRenderQueue::AddModel(const NzModel* model)
{
	#if NAZARA_GRAPHICS_SAFE
	if (!model)
	{
		NazaraError("Invalid model");
		return;
	}

	if (!model->IsDrawable())
	{
		NazaraError("Model is not drawable");
		return;
	}
	#endif

	const NzMatrix4f& transformMatrix = model->GetTransformMatrix();

	NzMesh* mesh = model->GetMesh();
	unsigned int submeshCount = mesh->GetSubMeshCount();

	for (unsigned int i = 0; i < submeshCount; ++i)
	{
		NzSubMesh* subMesh = mesh->GetSubMesh(i);
		NzMaterial* material = model->GetMaterial(subMesh->GetMaterialIndex());

		AddSubMesh(material, subMesh, transformMatrix);
	}
}

void NzDeferredRenderQueue::AddSprite(const NzSprite* sprite)
{
	#if NAZARA_GRAPHICS_SAFE
	if (!sprite)
	{
		NazaraError("Invalid sprite");
		return;
	}

	if (!sprite->IsDrawable())
	{
		NazaraError("Sprite is not drawable");
		return;
	}
	#endif

	NzMaterial* material = sprite->GetMaterial();
	if (material->IsEnabled(nzRendererParameter_Blend))
		m_forwardQueue->AddSprite(sprite);
	else
		sprites[material].push_back(sprite);
}

void NzDeferredRenderQueue::AddSubMesh(const NzMaterial* material, const NzSubMesh* subMesh, const NzMatrix4f& transformMatrix)
{
	switch (subMesh->GetAnimationType())
	{
		case nzAnimationType_Skeletal:
		{
			///TODO
			/*
			** Il y a ici deux choses importantes à gérer:
			** -Pour commencer, la mise en cache de std::vector suffisamment grands pour contenir le résultat du skinning
			**  l'objectif ici est d'éviter une allocation à chaque frame, donc de réutiliser un tableau existant
			**  Note: Il faudrait évaluer aussi la possibilité de conserver le buffer d'une frame à l'autre.
			**        Ceci permettant de ne pas skinner inutilement ce qui ne bouge pas, ou de skinner partiellement un mesh.
			**        Il faut cependant voir où stocker ce set de buffers, qui doit être communs à toutes les RQ d'une même scène.
			**
			** -Ensuite, la possibilité de regrouper les modèles skinnés identiques, une centaine de soldats marchant au pas
			**  ne devrait requérir qu'un skinning.
			*/
			NazaraError("Skeletal mesh not supported yet, sorry");
			break;
		}

		case nzAnimationType_Static:
		{
			if (material->IsEnabled(nzRendererParameter_Blend))
				m_forwardQueue->AddSubMesh(material, subMesh, transformMatrix);
			else
			{
				const NzStaticMesh* staticMesh = static_cast<const NzStaticMesh*>(subMesh);

				auto pair = opaqueModels.insert(std::make_pair(material, BatchedModelContainer::mapped_type()));
				if (pair.second)
					material->AddResourceListener(this, ResourceType_Material);

				bool& used = std::get<0>(pair.first->second);
				bool& enableInstancing = std::get<1>(pair.first->second);

				used = true;

				auto& meshMap = std::get<3>(pair.first->second);

				auto pair2 = meshMap.insert(std::make_pair(staticMesh, BatchedStaticMeshContainer::mapped_type()));
				if (pair2.second)
					staticMesh->AddResourceListener(this, ResourceType_StaticMesh);

				std::vector<StaticData>& staticDataContainer = pair2.first->second;

				unsigned int instanceCount = staticDataContainer.size() + 1;

				// Avons-nous suffisamment d'instances pour que le coût d'utilisation de l'instancing soit payé ?
				if (instanceCount >= NAZARA_GRAPHICS_INSTANCING_MIN_INSTANCES_COUNT)
					enableInstancing = true; // Apparemment oui, activons l'instancing avec ce matériau

				staticDataContainer.resize(instanceCount);
				StaticData& data = staticDataContainer.back();
				data.transformMatrix = transformMatrix;
			}

			break;
		}
	}
}

void NzDeferredRenderQueue::Clear(bool fully)
{
	directionalLights.clear();
	pointLights.clear();
	spotLights.clear();

	if (fully)
	{
		for (auto& matIt : opaqueModels)
		{
			const NzMaterial* material = matIt.first;
			material->RemoveResourceListener(this);

			BatchedSkeletalMeshContainer& skeletalContainer = std::get<2>(matIt.second);
			for (auto& meshIt : skeletalContainer)
			{
				const NzSkeletalMesh* skeletalMesh = meshIt.first;
				skeletalMesh->RemoveResourceListener(this);
			}

			BatchedStaticMeshContainer& staticContainer = std::get<3>(matIt.second);
			for (auto& meshIt : staticContainer)
			{
				const NzStaticMesh* staticMesh = meshIt.first;
				staticMesh->RemoveResourceListener(this);
			}
		}

		opaqueModels.clear();
		sprites.clear();
	}

	m_forwardQueue->Clear(fully);
}

bool NzDeferredRenderQueue::OnResourceDestroy(const NzResource* resource, int index)
{
	switch (index)
	{
		case ResourceType_Material:
			opaqueModels.erase(static_cast<const NzMaterial*>(resource));
			break;

		case ResourceType_SkeletalMesh:
		{
			for (auto& pair : opaqueModels)
				std::get<2>(pair.second).erase(static_cast<const NzSkeletalMesh*>(resource));

			break;
		}

		case ResourceType_StaticMesh:
		{
			for (auto& pair : opaqueModels)
				std::get<3>(pair.second).erase(static_cast<const NzStaticMesh*>(resource));

			break;
		}
	}

	return false; // Nous ne voulons plus recevoir d'évènement de cette ressource
}

void NzDeferredRenderQueue::OnResourceReleased(const NzResource* resource, int index)
{
	OnResourceDestroy(resource, index);
}

bool NzDeferredRenderQueue::BatchedModelMaterialComparator::operator()(const NzMaterial* mat1, const NzMaterial* mat2)
{
	nzUInt32 possibleFlags[] = {
		nzShaderFlags_Deferred,
		nzShaderFlags_Deferred | nzShaderFlags_Instancing
	};

	for (nzUInt32 flag : possibleFlags)
	{
		const NzShaderProgram* program1 = mat1->GetShaderProgram(nzShaderTarget_Model, flag);
		const NzShaderProgram* program2 = mat2->GetShaderProgram(nzShaderTarget_Model, flag);

		if (program1 != program2)
			return program1 < program2;
	}

	const NzTexture* diffuseMap1 = mat1->GetDiffuseMap();
	const NzTexture* diffuseMap2 = mat2->GetDiffuseMap();
	if (diffuseMap1 != diffuseMap2)
		return diffuseMap1 < diffuseMap2;

	return mat1 < mat2;
}

bool NzDeferredRenderQueue::BatchedSpriteMaterialComparator::operator()(const NzMaterial* mat1, const NzMaterial* mat2)
{
	nzUInt32 possibleFlags[] = {
		nzShaderFlags_Deferred
	};

	for (nzUInt32 flag : possibleFlags)
	{
		const NzShaderProgram* program1 = mat1->GetShaderProgram(nzShaderTarget_Model, flag);
		const NzShaderProgram* program2 = mat2->GetShaderProgram(nzShaderTarget_Model, flag);

		if (program1 != program2)
			return program1 < program2;
	}

	const NzTexture* diffuseMap1 = mat1->GetDiffuseMap();
	const NzTexture* diffuseMap2 = mat2->GetDiffuseMap();
	if (diffuseMap1 != diffuseMap2)
		return diffuseMap1 < diffuseMap2;

	return mat1 < mat2;
}

bool NzDeferredRenderQueue::BatchedSkeletalMeshComparator::operator()(const NzSkeletalMesh* subMesh1, const NzSkeletalMesh* subMesh2)
{
	const NzIndexBuffer* iBuffer1 = subMesh1->GetIndexBuffer();
	const NzBuffer* buffer1 = (iBuffer1) ? iBuffer1->GetBuffer() : nullptr;

	const NzIndexBuffer* iBuffer2 = subMesh1->GetIndexBuffer();
	const NzBuffer* buffer2 = (iBuffer2) ? iBuffer2->GetBuffer() : nullptr;

	if (buffer1 == buffer2)
		return subMesh1 < subMesh2;
	else
		return buffer2 < buffer2;
}

bool NzDeferredRenderQueue::BatchedStaticMeshComparator::operator()(const NzStaticMesh* subMesh1, const NzStaticMesh* subMesh2)
{
	const NzIndexBuffer* iBuffer1 = subMesh1->GetIndexBuffer();
	const NzBuffer* buffer1 = (iBuffer1) ? iBuffer1->GetBuffer() : nullptr;

	const NzIndexBuffer* iBuffer2 = subMesh2->GetIndexBuffer();
	const NzBuffer* buffer2 = (iBuffer2) ? iBuffer2->GetBuffer() : nullptr;

	if (buffer1 == buffer2)
	{
		buffer1 = subMesh1->GetVertexBuffer()->GetBuffer();
		buffer2 = subMesh2->GetVertexBuffer()->GetBuffer();

		if (buffer1 == buffer2)
			return subMesh1 < subMesh2;
		else
			return buffer1 < buffer2;
	}
	else
		return buffer1 < buffer2;
}
