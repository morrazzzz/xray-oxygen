////////////////////////////////////////////////////////////////////////////
//	Module 		: debug_text_tree.cpp
//	Created 	: 02.04.2008
//  Modified 	: 03.04.2008
//	Author		: Lain
//	Description : Text tree for onscreen debugging 
////////////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "debug_text_tree.h"

#include "Level.h"
#include "../xrUICore/ui_base.h"

namespace debug {

//-----------------------------------------------
// text_tree output
//-----------------------------------------------

namespace DebugTextTreeDetails
{
	struct texttree_draw_helper
	{
		struct params
		{
			int      color1;
			int      color2;
			int      max_rows;
			int      cur_row;
			int      column_size;
			int      ori_x;
			int      ori_y;
			int      offs;
		};

		static params s_params;

		void operator () (const char* s, int num_siblings)
		{
			if ( s_params.offs )
			{
				s_params.offs--;
				return;
			}

			if ( !s_params.cur_row )
			{
				UI().Font().GetFont("stat_font")->OutSet((float)s_params.ori_x, (float)s_params.ori_y);
			}			

			if ( s_params.cur_row % 2 )
			{
				UI().Font().GetFont("stat_font")->SetColor(s_params.color1);
			}
			else
			{
				UI().Font().GetFont("stat_font")->SetColor(s_params.color2);
			}
			
			UI().Font().GetFont("stat_font")->OutNext(s);
			s_params.cur_row++;
		}
	};

	texttree_draw_helper::params texttree_draw_helper::s_params;

	struct texttree_log_helper
	{
		void operator () (const char* s, int num_siblings)
		{
			Msg(s);
		}
	};
	
} // namespace detail

void   draw_text_tree (text_tree& tree, 
					   int        indent, // in spaces
					   int        ori_x,
					   int        ori_y,
					   int        offs,   // skip offs lines
					   int        column_size, // in pixels
					   int        max_rows,
					   u32        color1, 
					   u32        color2)
{
	DebugTextTreeDetails::texttree_draw_helper::s_params.color1      = color1;
	DebugTextTreeDetails::texttree_draw_helper::s_params.color2      = color2;
	DebugTextTreeDetails::texttree_draw_helper::s_params.column_size = column_size;
	DebugTextTreeDetails::texttree_draw_helper::s_params.cur_row     = 0;
	DebugTextTreeDetails::texttree_draw_helper::s_params.ori_x       = ori_x;
	DebugTextTreeDetails::texttree_draw_helper::s_params.ori_y       = ori_y;
	DebugTextTreeDetails::texttree_draw_helper::s_params.offs        = offs;
	DebugTextTreeDetails::texttree_draw_helper::s_params.max_rows    = max_rows;

	tree.output(DebugTextTreeDetails::texttree_draw_helper(), indent);
}

void   log_text_tree (text_tree& tree)
{
	tree.output(DebugTextTreeDetails::texttree_log_helper(), 2);
}

//-----------------------------------------------
// text_tree
//-----------------------------------------------

text_tree*   text_tree::find_node (const xr_string& s1)
{
	if ( strings.size() && (*strings.begin()) == s1 )
	{
		return this;
	}
	
	for ( Children::iterator i=children.begin(); i!=children.end(); ++i )
	{
		if ( text_tree* p = (*i)->find_node(s1) )
		{
			return p;
		}
	}

	return NULL;
}

text_tree&   text_tree::find_or_add (const xr_string& s1)
{
	if ( text_tree* p = find_node(s1) )
	{
		return *p;
	}

	return add_line(s1);
}

void   text_tree::toggle_show (int group_id_)
{
	if ( group_id == group_id_ )
	{
		shown = !shown;
	}
}

text_tree&   text_tree::add_line ()
{
	text_tree* child = xr_new<text_tree>();
	children.push_back(child);
	return *child;
}

void   text_tree::clear ()
{
	strings.clear();
	for_each(children.begin(), children.end(), &deleter);
	children.clear();
}

void   text_tree::prepare (int current_indent, int indent, Columns& columns)
{
	num_siblings = 1; // including ourselves

	for ( Children::iterator i=children.begin(); i!=children.end(); ++i )
	{
		if ( (*i)->shown )
		{
			(*i)->prepare(current_indent+indent, indent, columns);
			num_siblings += (*i)->num_siblings;
		}
	}

	if ( columns.size() < strings.size() )
	{
		columns.resize(strings.size());
	}

	Strings::iterator j = strings.begin();
	Columns::iterator c = columns.begin();

	// only count as column if theres more then 1 on the line!
	if ( strings.size() > 1 )
	{
		for ( ; j!=strings.end(); ++j, ++c )
		{
			int string_size = (int)(*j).size();
			string_size += (j==strings.begin()) ? current_indent : 0;				

			*c = std::max(string_size, *c);
		}
	}
}

} // namespace debug