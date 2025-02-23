// UIMapSpot.h:  ����� �� �����, 
// ��� ������� ���� �������� ��������� ��������
//////////////////////////////////////////////////////////////////////

#pragma once
/*
#include "UIStatic.h"
#include "UIButton.h"
#include "UIColorAnimatorWrapper.h"
#include "../InfoPortion.h"
#include "../game_graph.h"

//////////////////////////////////////////////////////////////////////////

#define					MAP_ICON_WIDTH			32
#define					MAP_ICON_HEIGHT			32

#define					MAP_ICON_GRID_WIDTH		32
#define					MAP_ICON_GRID_HEIGHT	32

extern const int		ARROW_DIMENTIONS;

//////////////////////////////////////////////////////////////////////////

class CUIMapSpot: public CUIButton
{
private:
	typedef CUIButton inherited;
public:
							CUIMapSpot	();
	virtual					~CUIMapSpot	();

	virtual void			Draw		();
	virtual void			Update		();
	virtual void			SendMessage	(CUIWindow* pWnd, s16 msg, void* pData );

	//���������� ������� ������� �� ����� � ������� �����������
	virtual Fvector			MapPos		();

	virtual void			SetObjectID	(u16 id);
	// ��������� ������������� ���������� � ��������� ������
	void					DynamicManifestation(bool value);

private:
	//id �������� ������
	GameGraph::_LEVEL_ID	m_our_level_id;
	//id �������, ������� ������������
	//�� �����, ���� ������ ��� �� 0xffff
	u16						m_object_id;

public:
	//������� ����� � ������� ������� ���������,
	//����� ���� m_pObject NULL
	Fvector					m_vWorldPos;
	//��� �������
	CUIString				m_sNameText;
	//���� ���� ����� � ��������� 
	CUIString				m_sDescText;
	// ���/���� �������-���������
	bool					m_bArrowEnabled;
	// ������� ��������� ���������
	bool					m_bArrowVisible;
	//���� ��������� ��������� ��� �������
	u32						arrow_color;
	
	//������������� ������ 
	enum EMapSpotAlign
	{
		eBottom, eCenter, eNone
	};
	EMapSpotAlign			m_eAlign;
	
	//����������� ������
	bool					m_bHeading;
	float					m_fHeading;
	// C�������� - ���������
	CUIStaticItem			m_Arrow;
	// ��� ������ �������� ����������� ����
	shared_str				m_LevelName;

protected:
	// �������� ��� �������� ��� ��� �������������/���������������
	CUIColorAnimatorWrapper	m_MapSpotAnimation;

public:
	// ��������� �������� �������� ��� ��������
	void					SetColorAnimation	(const shared_str &animationName) { m_MapSpotAnimation.SetColorAnimation(animationName); }
};
*/