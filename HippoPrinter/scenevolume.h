#pragma once

#include <map>

#include <src/slic3r/GUI/3DScene.hpp>
class SceneVolume
{
public:
	SceneVolume();
	~SceneVolume();

	const BoundingBoxf3& BBox()const;
	BoundingBoxf3& BBox();
	BoundingBoxf3 TransformedBBox();
	void SetBBox(const BoundingBoxf3& bbox);


	const Pointf3& Origin()const;
	Pointf3& Origin();
	void SetOrigin(const Pointf3& origin);

	void SetSelected(bool flag);
	bool Selected() const;

	void SetHover(bool flag);
	bool Hover() const;

	void LoadMesh(const TriangleMesh& mesh);
	
// 	const GLVertexArray& Verts()const;
// 	GLVertexArray& Verts();
// 	void SetVerts(const GLVertexArray& verts);

public:
	BoundingBoxf3 bbox_;
	Pointf3 origin_;
	bool selected_;
	bool hover_;
	int select_group_id_;
	int drag_group_id_;
	float color[4];
	GLVertexArray tverts_;
	GLVertexArray qverts_;


	std::map<double, std::pair<int, int> > offsets;
};

