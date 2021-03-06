#ifdef HAVE_CONFIG_H
# include "elementary_config.h"
#endif
#define EFL_UI_FOCUS_OBJECT_PROTECTED


#include <Elementary.h>
#include "elm_priv.h"

#define MY_CLASS EFL_UI_FOCUS_MANAGER_CALC_CLASS
#define FOCUS_DATA(obj) Efl_Ui_Focus_Manager_Calc_Data *pd = efl_data_scope_get(obj, MY_CLASS);

#define NODE_DIRECTIONS_COUNT 4

#define DIRECTION_IS_LOGICAL(dir) (dir >= EFL_UI_FOCUS_DIRECTION_PREVIOUS && dir < EFL_UI_FOCUS_DIRECTION_UP)
#define DIRECTION_IS_2D(dir) (dir >= EFL_UI_FOCUS_DIRECTION_UP && dir < EFL_UI_FOCUS_DIRECTION_LAST)
#define DIRECTION_CHECK(dir) (dir >= EFL_UI_FOCUS_DIRECTION_PREVIOUS && dir < EFL_UI_FOCUS_DIRECTION_LAST)

//#define CALC_DEBUG
#define DEBUG_TUPLE(obj) efl_name_get(obj), efl_class_name_get(obj)

static int _focus_log_domain = -1;

#define F_CRI(...) EINA_LOG_DOM_CRIT(_focus_log_domain, __VA_ARGS__)
#define F_ERR(...) EINA_LOG_DOM_ERR(_focus_log_domain, __VA_ARGS__)
#define F_WRN(...) EINA_LOG_DOM_WARN(_focus_log_domain, __VA_ARGS__)
#define F_INF(...) EINA_LOG_DOM_INFO(_focus_log_domain, __VA_ARGS__)
#define F_DBG(...) EINA_LOG_DOM_DBG(_focus_log_domain, __VA_ARGS__)

#define DIRECTION_ACCESS(V, ID) ((V)->graph.directions[(ID) - 2])

typedef enum {
    DIMENSION_X = 0,
    DIMENSION_Y = 1,
} Dimension;

typedef struct _Border Border;
typedef struct _Node Node;

struct _Border {
  Eina_List *partners; //partners that are linked in both directions
  Eina_List *one_direction; //partners that are linked in one direction
  Eina_List *cleanup_nodes; //a list of nodes that needs to be cleaned up when this node is deleted
};

typedef enum {
  NODE_TYPE_NORMAL = 0,
  NODE_TYPE_ONLY_LOGICAL = 2,
} Node_Type;

struct _Node{
  Node_Type type; //type of the node

  Efl_Ui_Focus_Object *focusable;
  Efl_Ui_Focus_Manager *manager;
  Efl_Ui_Focus_Manager *redirect_manager;

  struct _Tree_Node{
    Node *parent; //the parent of the tree
    Eina_List *children; //this saves the original set of elements
    Eina_List *saved_order;
  }tree;

  struct _Graph_Node {
    Border directions[NODE_DIRECTIONS_COUNT];
  } graph;
};

#define T(n) (n->tree)
#define G(n) (n->graph)

typedef struct {
    Eina_List *focus_stack;
    Eina_Hash *node_hash;
    Efl_Ui_Focus_Manager *redirect;
    Eina_List *dirty;

    Node *root;
} Efl_Ui_Focus_Manager_Calc_Data;

static void
_manager_in_chain_set(Eo *obj, Efl_Ui_Focus_Manager_Calc_Data *pd)
{
   EINA_SAFETY_ON_NULL_RETURN(pd->root);

   if (!efl_isa(pd->root->focusable, EFL_UI_WIN_CLASS))
     EINA_SAFETY_ON_NULL_RETURN(efl_ui_focus_user_focus_manager_get(pd->root->focusable));

   //so we dont run infinitly this does not fix it, but at least we only have a error
   EINA_SAFETY_ON_TRUE_RETURN(efl_ui_focus_user_focus_manager_get(pd->root->focusable) == obj);

   efl_ui_focus_manager_focus_set(efl_ui_focus_user_focus_manager_get(pd->root->focusable), pd->root->focusable);
}

static Efl_Ui_Focus_Direction
_complement(Efl_Ui_Focus_Direction dir)
{
    #define COMP(a,b) \
        if (dir == a) return b; \
        if (dir == b) return a;

    COMP(EFL_UI_FOCUS_DIRECTION_RIGHT, EFL_UI_FOCUS_DIRECTION_LEFT)
    COMP(EFL_UI_FOCUS_DIRECTION_UP, EFL_UI_FOCUS_DIRECTION_DOWN)
    COMP(EFL_UI_FOCUS_DIRECTION_PREVIOUS, EFL_UI_FOCUS_DIRECTION_NEXT)

    #undef COMP

    return EFL_UI_FOCUS_DIRECTION_LAST;
}

/*
 * Set this new list of partners to the border.
 * All old partners will be deleted
 */
static void
border_partners_set(Node *node, Efl_Ui_Focus_Direction direction, Eina_List *list)
{
   Node *partner;
   Eina_List *lnode;
   Border *border;

   EINA_SAFETY_ON_FALSE_RETURN(DIRECTION_IS_2D(direction));

   border = &DIRECTION_ACCESS(node, direction);

   EINA_LIST_FREE(border->partners, partner)
     {
        Border *comp_border = &DIRECTION_ACCESS(partner, _complement(direction));

        comp_border->partners = eina_list_remove(comp_border->partners, node);
     }

   border->partners = list;

   EINA_LIST_FOREACH(border->partners, lnode, partner)
     {
        Border *comp_border = &DIRECTION_ACCESS(partner,_complement(direction));

        comp_border->partners = eina_list_append(comp_border->partners, node);
     }
}

static void
border_onedirection_set(Node *node, Efl_Ui_Focus_Direction direction, Eina_List *list)
{
   Node *partner;
   Eina_List *lnode;
   Border *border;

   EINA_SAFETY_ON_FALSE_RETURN(DIRECTION_IS_2D(direction));

   border = &DIRECTION_ACCESS(node, direction);

   EINA_LIST_FREE(border->one_direction, partner)
     {
        Border *b = &DIRECTION_ACCESS(partner, _complement(direction));
        b->cleanup_nodes = eina_list_remove(b->cleanup_nodes, node);
     }

   border->one_direction = list;

   EINA_LIST_FOREACH(border->one_direction, lnode, partner)
     {
        Border *comp_border = &DIRECTION_ACCESS(partner,_complement(direction));

        comp_border->cleanup_nodes = eina_list_append(comp_border->cleanup_nodes, node);
     }
}

static void
border_onedirection_cleanup(Node *node, Efl_Ui_Focus_Direction direction)
{
   Node *partner;
   Border *border;

   EINA_SAFETY_ON_FALSE_RETURN(DIRECTION_IS_2D(direction));

   border = &DIRECTION_ACCESS(node, direction);

   EINA_LIST_FREE(border->cleanup_nodes, partner)
     {
        Border *b = &DIRECTION_ACCESS(partner, _complement(direction));
        b->one_direction = eina_list_remove(b->one_direction, node);
     }
}
/**
 * Create a new node
 */
static Node*
node_new(Efl_Ui_Focus_Object *focusable, Efl_Ui_Focus_Manager *manager)
{
    Node *node;

    node = calloc(1, sizeof(Node));

    node->focusable = focusable;
    node->manager = manager;

    return node;
}

/**
 * Looks up given focus object from the focus manager.
 *
 * @returns node found, or NULL if focusable was not found in the manager.
 */
static Node*
node_get(Efl_Ui_Focus_Manager *obj, Efl_Ui_Focus_Manager_Calc_Data *pd, Efl_Ui_Focus_Object *focusable)
{
   Node *ret;

   ret = eina_hash_find(pd->node_hash, &focusable);

   if (ret) return ret;

   ERR("Focusable %p (%s) not registered in manager %p", focusable, efl_class_name_get(focusable), obj);

   return NULL;
}

/**
 * Free a node item and unlink this item from all direction
 */
static void
node_item_free(Node *item)
{
   Node *n;
   Eina_List *l;
   //free the graph items
   for(int i = EFL_UI_FOCUS_DIRECTION_UP;i < EFL_UI_FOCUS_DIRECTION_LAST; i++)
     {
        border_partners_set(item, i, NULL);
        border_onedirection_cleanup(item, i);
        border_onedirection_set(item, i, NULL);
     }

   //free the tree items
   if (!item->tree.parent && item->tree.children)
     {
        ERR("Freeing the root with children is going to break the logical tree!");
     }

   if (item->tree.parent && item->tree.children)
     {
        Node *parent;

        parent = item->tree.parent;
        //reparent everything into the next layer
        EINA_LIST_FOREACH(item->tree.children, l, n)
          {
             n->tree.parent = item->tree.parent;
          }
        parent->tree.children = eina_list_merge(parent->tree.children , item->tree.children);
     }

   if (item->tree.parent)
     {
        Node *parent;

        parent = item->tree.parent;
        T(parent).children = eina_list_remove(T(parent).children, item);
     }

   //free the safed order
   ELM_SAFE_FREE(T(item).saved_order, eina_list_free);

   free(item);
}
//FOCUS-STACK HELPERS

static Efl_Ui_Focus_Object*
_focus_stack_unfocus_last(Efl_Ui_Focus_Manager_Calc_Data *pd)
{
   Efl_Ui_Focus_Object *focusable = NULL;
   Node *n;

   n = eina_list_last_data_get(pd->focus_stack);

   if (n)
     focusable = n->focusable;

   pd->focus_stack = eina_list_remove(pd->focus_stack, n);

   if (n)
     efl_ui_focus_object_focus_set(n->focusable, EINA_FALSE);

   return focusable;
}

static Node*
_focus_stack_last_regular(Efl_Ui_Focus_Manager_Calc_Data *pd)
{
   Eina_List *l;
   Node *upper;

   l = eina_list_last(pd->focus_stack);
   upper = eina_list_last_data_get(pd->focus_stack);

   while (upper && upper->type != NODE_TYPE_NORMAL)
     {
        l = eina_list_prev(l);
        upper = eina_list_data_get(l);
     }

   return upper;
}

//CALCULATING STUFF

static inline int
_distance(Eina_Rect node, Eina_Rect op, Dimension dim)
{
    int min, max, point;
    int v1, v2;

    if (dim == DIMENSION_X)
      {
         min = op.x;
         max = eina_rectangle_max_x(&op.rect);
         point = node.x + node.w/2;
      }
    else
      {
         min = op.y;
         max = eina_rectangle_max_y(&op.rect);
         point = node.y + node.h/2;
      }

    v1 = min - point;
    v2 = max - point;

    if (abs(v1) < abs(v2))
      return v1;
    else
      return v2;
}

static inline void
_min_max_gen(Dimension dim, Eina_Rect rect, int *min, int *max)
{
   if (dim == DIMENSION_X)
     {
        *min = rect.y;
        *max = eina_rectangle_max_y(&rect.rect);
     }
   else
     {
        *min = rect.x;
        *max = eina_rectangle_max_x(&rect.rect);
     }
}

static inline void
_calculate_node(Efl_Ui_Focus_Manager_Calc_Data *pd, Efl_Ui_Focus_Object *node, Dimension dim, Eina_List **pos, Eina_List **neg)
{
   int dim_min, dim_max, cur_pos_min = 0, cur_neg_min = 0;
   Efl_Ui_Focus_Object *op;
   Eina_Iterator *nodes;
   Eina_Rect rect;
   Node *n;

   *pos = NULL;
   *neg = NULL;

   rect = efl_ui_focus_object_focus_geometry_get(node);
   nodes = eina_hash_iterator_data_new(pd->node_hash);
   _min_max_gen(dim, rect, &dim_min, &dim_max);

   EINA_ITERATOR_FOREACH(nodes, n)
     {
        Eina_Rect op_rect;
        int min, max;

        op = n->focusable;
        if (op == node) continue;

        if (n->type == NODE_TYPE_ONLY_LOGICAL) continue;

        op_rect = efl_ui_focus_object_focus_geometry_get(op);

        _min_max_gen(dim, op_rect, &min, &max);

        /* The only way the calculation does make sense is if the two number
         * lines are not disconnected.
         * If they are connected one point of the 4 lies between the min and max of the other line
         */
        if (!((min <= max && max <= dim_min && dim_min <= dim_max) ||
              (dim_min <= dim_max && dim_max <= min && min <= max)) &&
              !eina_rectangle_intersection(&op_rect.rect, &rect.rect))
          {
             //this thing hits horizontal
             int tmp_dis;

             tmp_dis = _distance(rect, op_rect, dim);

             if (tmp_dis < 0)
               {
                  if (tmp_dis == cur_neg_min)
                    {
                       //add it
                       *neg = eina_list_append(*neg, op);
                    }
                  else if (tmp_dis > cur_neg_min
                    || cur_neg_min == 0) //init case
                    {
                       //nuke the old and add
#ifdef CALC_DEBUG
                       printf("CORRECTION FOR %s-%s\n found anchor %s-%s in distance %d\n (%d,%d,%d,%d)\n (%d,%d,%d,%d)\n\n", DEBUG_TUPLE(node), DEBUG_TUPLE(op),
                         tmp_dis,
                         op_rect.x, op_rect.y, op_rect.w, op_rect.h,
                         rect.x, rect.y, rect.w, rect.h);
#endif
                       *neg = eina_list_free(*neg);
                       *neg = eina_list_append(NULL, op);
                       cur_neg_min = tmp_dis;
                    }
               }
             else
               {
                  if (tmp_dis == cur_pos_min)
                    {
                       //add it
                       *pos = eina_list_append(*pos, op);
                    }
                  else if (tmp_dis < cur_pos_min
                    || cur_pos_min == 0) //init case
                    {
                       //nuke the old and add
#ifdef CALC_DEBUG
                       printf("CORRECTION FOR %s-%s\n found anchor %s-%s in distance %d\n (%d,%d,%d,%d)\n (%d,%d,%d,%d)\n\n", DEBUG_TUPLE(node), DEBUG_TUPLE(op),
                         tmp_dis,
                         op_rect.x, op_rect.y, op_rect.w, op_rect.h,
                         rect.x, rect.y, rect.w, rect.h);
#endif
                       *pos = eina_list_free(*pos);
                       *pos = eina_list_append(NULL, op);
                       cur_pos_min = tmp_dis;
                    }
               }


#if 0
             printf("(%d,%d,%d,%d)%s vs(%d,%d,%d,%d)%s\n", rect.x, rect.y, rect.w, rect.h, elm_widget_part_text_get(node, NULL), op_rect.x, op_rect.y, op_rect.w, op_rect.h, elm_widget_part_text_get(op, NULL));
             printf("(%d,%d,%d,%d)\n", min, max, dim_min, dim_max);
             printf("Candidate %d\n", tmp_dis);
             if (anchor->anchor == NULL || abs(tmp_dis) < abs(distance)) //init case
               {
                  distance = tmp_dis;
                  anchor->positive = tmp_dis > 0 ? EINA_FALSE : EINA_TRUE;
                  anchor->anchor = op;
                  //Helper for debugging wrong calculations

               }
#endif
         }


     }
   eina_iterator_free(nodes);
   nodes = NULL;

}

static inline Eina_Position2D
_relative_position_rects(Eina_Rect *a, Eina_Rect *b)
{
   Eina_Position2D a_pos = {a->rect.x + a->rect.w/2, a->rect.y + a->rect.h/2};
   Eina_Position2D b_pos = {b->rect.x + b->rect.w/2, b->rect.y + b->rect.h/2};

   return (Eina_Position2D){b_pos.x - a_pos.x, b_pos.y - b_pos.y};
}

static inline Eina_Rectangle_Outside
_direction_to_outside(Efl_Ui_Focus_Direction direction)
{
   if (direction == EFL_UI_FOCUS_DIRECTION_RIGHT) return EINA_RECTANGLE_OUTSIDE_RIGHT;
   if (direction == EFL_UI_FOCUS_DIRECTION_LEFT) return EINA_RECTANGLE_OUTSIDE_LEFT;
   if (direction == EFL_UI_FOCUS_DIRECTION_DOWN) return EINA_RECTANGLE_OUTSIDE_BOTTOM;
   if (direction == EFL_UI_FOCUS_DIRECTION_UP) return EINA_RECTANGLE_OUTSIDE_TOP;

   return -1;
}

static inline void
_calculate_node_indirection(Efl_Ui_Focus_Manager_Calc_Data *pd, Efl_Ui_Focus_Object *node, Efl_Ui_Focus_Direction direction, Eina_List **lst)
{
   Efl_Ui_Focus_Object *op;
   Eina_Iterator *nodes;
   int min_distance = 0;
   Node *n;
   Eina_Rect rect;

   rect = efl_ui_focus_object_focus_geometry_get(node);
   nodes = eina_hash_iterator_data_new(pd->node_hash);

   EINA_ITERATOR_FOREACH(nodes, n)
     {
        Eina_Rectangle_Outside outside, outside_dir;
        Eina_Position2D pos;
        int distance;
        Eina_Rect op_rect;

        op = n->focusable;

        if (op == node) continue;
        if (n->type == NODE_TYPE_ONLY_LOGICAL) continue;

        op_rect = efl_ui_focus_object_focus_geometry_get(op);
        outside = eina_rectangle_outside_position(&rect.rect, &op_rect.rect);
        outside_dir = _direction_to_outside(direction);
        //calculate relative position of the nodes
        pos = _relative_position_rects(&rect, &op_rect);
        //calculate distance
        distance = sqrt(powerof2(pos.x) + powerof2(pos.y));

        if (outside & outside_dir)
          {
             if (min_distance == 0 || min_distance > distance)
               {
                  min_distance = distance;
                  *lst = eina_list_free(*lst);
                  *lst = eina_list_append(*lst, op);
               }
             else if (min_distance == distance)
               {
                  *lst = eina_list_append(*lst, op);
               }
          }
     }
}

#ifdef CALC_DEBUG
static void
_debug_node(Node *node)
{
   Eina_List *tmp = NULL;

   if (!node) return;

   printf("NODE %s-%s\n", DEBUG_TUPLE(node->focusable));

#define DIR_LIST(dir) DIRECTION_ACCESS(node,dir).partners

#define DIR_OUT(dir)\
   tmp = DIR_LIST(dir); \
   { \
      Eina_List *list_node; \
      Node *partner; \
      printf("-"#dir"-> ("); \
      EINA_LIST_FOREACH(tmp, list_node, partner) \
        printf("%s-%s,", DEBUG_TUPLE(partner->focusable)); \
      printf(")\n"); \
   }

   DIR_OUT(EFL_UI_FOCUS_DIRECTION_RIGHT)
   DIR_OUT(EFL_UI_FOCUS_DIRECTION_LEFT)
   DIR_OUT(EFL_UI_FOCUS_DIRECTION_UP)
   DIR_OUT(EFL_UI_FOCUS_DIRECTION_DOWN)

}
#endif

static void
convert_set(Efl_Ui_Focus_Manager *obj, Efl_Ui_Focus_Manager_Calc_Data *pd, Node *node, Eina_List *focusable_list, Efl_Ui_Focus_Direction dir, void (*converter)(Node *node, Efl_Ui_Focus_Direction direction, Eina_List *list))
{
   Eina_List *partners = NULL;
   Efl_Ui_Focus_Object *fobj;

   EINA_LIST_FREE(focusable_list, fobj)
     {
        Node *entry;

        entry = node_get(obj, pd, fobj);
        if (!entry)
          {
             CRI("Found a obj in graph without node-entry!");
             return;
          }
        partners = eina_list_append(partners, entry);
     }

   converter(node, dir, partners);
}

static void
dirty_flush_node(Efl_Ui_Focus_Manager *obj EINA_UNUSED, Efl_Ui_Focus_Manager_Calc_Data *pd, Node *node)
{
   Eina_List *x_partners_pos, *x_partners_neg;
   Eina_List *y_partners_pos, *y_partners_neg;

   _calculate_node(pd, node->focusable, DIMENSION_X, &x_partners_pos, &x_partners_neg);
   _calculate_node(pd, node->focusable, DIMENSION_Y, &y_partners_pos, &y_partners_neg);

   convert_set(obj, pd, node, x_partners_pos, EFL_UI_FOCUS_DIRECTION_RIGHT, border_partners_set);
   convert_set(obj, pd, node, x_partners_neg, EFL_UI_FOCUS_DIRECTION_LEFT, border_partners_set);
   convert_set(obj, pd, node, y_partners_neg, EFL_UI_FOCUS_DIRECTION_UP, border_partners_set);
   convert_set(obj, pd, node, y_partners_pos, EFL_UI_FOCUS_DIRECTION_DOWN, border_partners_set);


   /*
    * Stage 2: if there is still no relation in a special direction,
    *          just take every single node that is in the given direction
    *          and take the one with the shortest direction
    */
   for(int i = EFL_UI_FOCUS_DIRECTION_UP; i < EFL_UI_FOCUS_DIRECTION_LAST; i++)
     {
        if (!DIRECTION_ACCESS(node, i).partners)
          {
             Eina_List *tmp = NULL;

             _calculate_node_indirection(pd, node->focusable, i, &tmp);
             convert_set(obj, pd, node, tmp, i, border_onedirection_set);
          }
     }

#ifdef CALC_DEBUG
   _debug_node(node);
#endif
}

static void
dirty_flush(Efl_Ui_Focus_Manager *obj, Efl_Ui_Focus_Manager_Calc_Data *pd, Node *node)
{
   if (!eina_list_data_find(pd->dirty, node)) return;

   efl_event_callback_call(obj, EFL_UI_FOCUS_MANAGER_EVENT_FLUSH_PRE, NULL);

   pd->dirty = eina_list_remove(pd->dirty, node);

   dirty_flush_node(obj, pd, node);
}

static void
dirty_flush_all(Efl_Ui_Focus_Manager *obj, Efl_Ui_Focus_Manager_Calc_Data *pd)
{
   Node *node;

   efl_event_callback_call(obj, EFL_UI_FOCUS_MANAGER_EVENT_FLUSH_PRE, NULL);

   EINA_LIST_FREE(pd->dirty, node)
     {
        dirty_flush_node(obj, pd, node);
     }
}

static void
dirty_add(Eo *obj, Efl_Ui_Focus_Manager_Calc_Data *pd, Node *dirty)
{
   if (dirty->type == NODE_TYPE_ONLY_LOGICAL)
     {
        ERR("Only not only logical nodes can be marked dirty");
        return;
     }

   //if (eina_list_data_find(pd->dirty, dirty)) return;
   pd->dirty = eina_list_remove(pd->dirty, dirty);
   pd->dirty = eina_list_append(pd->dirty, dirty);

   efl_event_callback_call(obj, EFL_UI_FOCUS_MANAGER_EVENT_COORDS_DIRTY, NULL);
}


static void
_node_new_geometry_cb(void *data, const Efl_Event *event)
{
   Node *node;
   FOCUS_DATA(data)

   node = node_get(data, pd, event->object);
   if (!node)
      return;

   dirty_add(data, pd, node);

   return;
}

static void
_object_del_cb(void *data, const Efl_Event *event)
{
   /*
    * Lets just implicitly delete items that are deleted
    * Otherwise we have later just a bunch of errors
    */
   efl_ui_focus_manager_calc_unregister(data, event->object);
}

EFL_CALLBACKS_ARRAY_DEFINE(focusable_node,
    {EFL_GFX_EVENT_RESIZE, _node_new_geometry_cb},
    {EFL_GFX_EVENT_MOVE, _node_new_geometry_cb},
    {EFL_EVENT_DEL, _object_del_cb},
);

//=============================

static Node*
_register(Eo *obj, Efl_Ui_Focus_Manager_Calc_Data *pd, Efl_Ui_Focus_Object *child, Node *parent)
{
   Node *node;
   node = eina_hash_find(pd->node_hash, &child);
   if (node)
     {
        ERR("Child %p is already registered in the graph (%s)", child, node->type == NODE_TYPE_ONLY_LOGICAL ? "logical" : "regular");
        return NULL;
     }

   node = node_new(child, obj);
   eina_hash_add(pd->node_hash, &child, node);

   //add the parent
   if (parent)
     {
        T(node).parent = parent;
        T(parent).children = eina_list_append(T(parent).children, node);
     }

   return node;
}
EOLIAN static Eina_Bool
_efl_ui_focus_manager_calc_register_logical(Eo *obj, Efl_Ui_Focus_Manager_Calc_Data *pd, Efl_Ui_Focus_Object *child, Efl_Ui_Focus_Object *parent,  Efl_Ui_Focus_Manager *redirect)
{
   Node *node = NULL;
   Node *pnode = NULL;

   EINA_SAFETY_ON_NULL_RETURN_VAL(child, EINA_FALSE);
   EINA_SAFETY_ON_NULL_RETURN_VAL(parent, EINA_FALSE);

   if (redirect)
     EINA_SAFETY_ON_FALSE_RETURN_VAL(efl_isa(redirect, EFL_UI_FOCUS_MANAGER_INTERFACE), EINA_FALSE);

   F_DBG("Manager: %p register %p %p %p", obj, child, parent, redirect);

   pnode = node_get(obj, pd, parent);
   if (!pnode) return EINA_FALSE;

   node = _register(obj, pd, child, pnode);
   if (!node) return EINA_FALSE;

   node->type = NODE_TYPE_ONLY_LOGICAL;
   node->redirect_manager = redirect;

   //set again
   if (T(pnode).saved_order)
     {
        Eina_List *tmp;

        tmp = eina_list_clone(T(pnode).saved_order);
        efl_ui_focus_manager_calc_update_order(obj, parent, tmp);
     }

   return EINA_TRUE;
}


EOLIAN static Eina_Bool
_efl_ui_focus_manager_calc_register(Eo *obj, Efl_Ui_Focus_Manager_Calc_Data *pd, Efl_Ui_Focus_Object *child, Efl_Ui_Focus_Object *parent, Efl_Ui_Focus_Manager *redirect)
{
   Node *node = NULL;
   Node *pnode = NULL;

   EINA_SAFETY_ON_NULL_RETURN_VAL(child, EINA_FALSE);
   EINA_SAFETY_ON_NULL_RETURN_VAL(parent, EINA_FALSE);

   if (redirect)
     EINA_SAFETY_ON_FALSE_RETURN_VAL(efl_isa(redirect, EFL_UI_FOCUS_MANAGER_INTERFACE), EINA_FALSE);

   F_DBG("Manager: %p register %p %p %p", obj, child, parent, redirect);

   pnode = node_get(obj, pd, parent);
   if (!pnode) return EINA_FALSE;

   node = _register(obj, pd, child, pnode);
   if (!node) return EINA_FALSE;

   //listen to changes
   efl_event_callback_array_add(child, focusable_node(), obj);

   node->type = NODE_TYPE_NORMAL;
   node->redirect_manager = redirect;

   //mark dirty
   dirty_add(obj, pd, node);

   //set again
   if (T(pnode).saved_order)
     {
        Eina_List *tmp;

        tmp = eina_list_clone(T(pnode).saved_order);
        efl_ui_focus_manager_calc_update_order(obj, parent, tmp);
     }

   return EINA_TRUE;
}

EOLIAN static Eina_Bool
_efl_ui_focus_manager_calc_update_redirect(Eo *obj, Efl_Ui_Focus_Manager_Calc_Data *pd, Efl_Ui_Focus_Object *child, Efl_Ui_Focus_Manager *redirect)
{
   Node *node = node_get(obj, pd, child);
   if (!node) return EINA_FALSE;

   if (redirect)
     EINA_SAFETY_ON_FALSE_RETURN_VAL(efl_isa(redirect, MY_CLASS), EINA_FALSE);

   node->redirect_manager = redirect;

   return EINA_TRUE;
}

EOLIAN static Eina_Bool
_efl_ui_focus_manager_calc_update_parent(Eo *obj EINA_UNUSED, Efl_Ui_Focus_Manager_Calc_Data *pd, Efl_Ui_Focus_Object *child, Efl_Ui_Focus_Object *parent_obj)
{
   Node *node;
   Node *parent;

   EINA_SAFETY_ON_NULL_RETURN_VAL(parent_obj, EINA_FALSE);
   EINA_SAFETY_ON_NULL_RETURN_VAL(child, EINA_FALSE);

   node = node_get(obj, pd, child);
   parent = node_get(obj, pd, parent_obj);

   if (!node || !parent) return EINA_FALSE;

   if (T(node).parent)
     {
        Node *old_parent;

        old_parent = T(node).parent;

        T(old_parent).children = eina_list_remove(T(old_parent).children, node);
     }

   T(node).parent = parent;

   if (T(node).parent)
     {
        T(parent).children = eina_list_append(T(parent).children, node);
     }

   return EINA_TRUE;
}

static Eina_List*
_set_a_without_b(Eina_List *a, Eina_List *b)
{
   Eina_List *a_out = NULL, *node;
   void *data;

   a_out = eina_list_clone(a);

   EINA_LIST_FOREACH(b, node, data)
     {
        a_out = eina_list_remove(a_out, data);
     }

   return a_out;
}

static Eina_Bool
_equal_set(Eina_List *none_nodes, Eina_List *nodes)
{
   Eina_List *n;
   Node *node;

   if (eina_list_count(nodes) != eina_list_count(none_nodes)) return EINA_FALSE;

   EINA_LIST_FOREACH(nodes, n, node)
     {
        if (!eina_list_data_find(none_nodes, node))
          return EINA_FALSE;
     }

   return EINA_TRUE;
}

EOLIAN static void
_efl_ui_focus_manager_calc_update_order(Eo *obj, Efl_Ui_Focus_Manager_Calc_Data *pd, Efl_Ui_Focus_Object *parent, Eina_List *order)
{
   Node *pnode;
   Efl_Ui_Focus_Object *o;
   Eina_List *node_order = NULL, *not_ordered, *trash, *node_order_clean, *n;

   F_DBG("Manager_update_order on %p %p", obj, parent);

   pnode = node_get(obj, pd, parent);
   if (!pnode)
     return;

   ELM_SAFE_FREE(T(pnode).saved_order, eina_list_free);
   T(pnode).saved_order = order;

   //get all nodes from the subset
   EINA_LIST_FOREACH(order, n, o)
     {
        Node *tmp;

        tmp = eina_hash_find(pd->node_hash, &o);

        if (!tmp) continue;

        node_order = eina_list_append(node_order, tmp);
     }

   not_ordered = _set_a_without_b(T(pnode).children, node_order);
   trash = _set_a_without_b(node_order, T(pnode).children);
   node_order_clean = _set_a_without_b(node_order, trash);

   eina_list_free(node_order);
   eina_list_free(trash);

   eina_list_free(T(pnode).children);
   T(pnode).children = eina_list_merge(node_order_clean, not_ordered);

   return;
}

EOLIAN static Eina_Bool
_efl_ui_focus_manager_calc_update_children(Eo *obj EINA_UNUSED, Efl_Ui_Focus_Manager_Calc_Data *pd, Efl_Ui_Focus_Object *parent, Eina_List *order)
{
   Node *pnode;
   Efl_Ui_Focus_Object *o;
   Eina_Bool fail = EINA_FALSE;
   Eina_List *node_order = NULL;

   pnode = node_get(obj, pd, parent);
   if (!pnode)
     return EINA_FALSE;

   //get all nodes from the subset
   EINA_LIST_FREE(order, o)
     {
        Node *tmp;

        tmp = node_get(obj, pd, o);
        if (!tmp)
          fail = EINA_TRUE;
        node_order = eina_list_append(node_order, tmp);
     }

   if (fail)
     {
        eina_list_free(node_order);
        return EINA_FALSE;
     }

   if (!_equal_set(node_order, T(pnode).children))
     {
        ERR("Set of children is not equal");
        return EINA_FALSE;
     }

   eina_list_free(T(pnode).children);
   T(pnode).children = node_order;

   return EINA_TRUE;
}

EOLIAN static void
_efl_ui_focus_manager_calc_unregister(Eo *obj EINA_UNUSED, Efl_Ui_Focus_Manager_Calc_Data *pd, Efl_Ui_Focus_Object *child)
{
   Node *node;
   Eina_Bool refocus = EINA_FALSE;

   node = eina_hash_find(pd->node_hash, &child);

   if (!node) return;

   F_DBG("Manager: %p unregister %p", obj, child);

   if (eina_list_last_data_get(pd->focus_stack) == node)
     {
        //unfocus the current head
        efl_ui_focus_object_focus_set(child, EINA_FALSE);
        refocus = EINA_TRUE;
     }


   //remove the object from the stack if it hasn't done that until now
   //after this it's not at the top anymore
   //elm_widget_focus_set(node->focusable, EINA_FALSE);
   //delete again from the list, for the case it was not at the top
   pd->focus_stack = eina_list_remove(pd->focus_stack, node);

   if (refocus)
     {
        Node *n = eina_list_last_data_get(pd->focus_stack);
        if (n)
          efl_ui_focus_object_focus_set(n->focusable, EINA_TRUE);
     }

   //add all neighbors of the node to the dirty list
   for(int i = EFL_UI_FOCUS_DIRECTION_UP; i < EFL_UI_FOCUS_DIRECTION_LAST; i++)
     {
        Node *partner;
        Eina_List *n;

        EINA_LIST_FOREACH(DIRECTION_ACCESS(node, i).partners, n, partner)
          {
             dirty_add(obj, pd, partner);
          }
     }

   //remove from the dirty parts
   pd->dirty = eina_list_remove(pd->dirty, node);

   eina_hash_del_by_key(pd->node_hash, &child);
}

EOLIAN static void
_efl_ui_focus_manager_calc_efl_ui_focus_manager_redirect_set(Eo *obj, Efl_Ui_Focus_Manager_Calc_Data *pd, Efl_Ui_Focus_Manager *redirect)
{
   Efl_Ui_Focus_Manager *old_manager;

   if (pd->redirect == redirect) return;

   F_DBG("Manager: %p setting redirect from %p to %p", obj, pd->redirect, redirect);

   old_manager = pd->redirect;

   if (pd->redirect)
     efl_wref_del(pd->redirect, &pd->redirect);

   pd->redirect = redirect;

   if (pd->redirect)
     efl_wref_add(pd->redirect, &pd->redirect);

   efl_ui_focus_manager_reset_history(old_manager);

   //we might have saved a logical element at the top, remove that if there is one
   {
      Node *n = NULL;

      n = eina_list_last_data_get(pd->focus_stack);

      if (n && n->type == NODE_TYPE_ONLY_LOGICAL && n->redirect_manager == old_manager)
        pd->focus_stack = eina_list_remove(pd->focus_stack, n);
   }

   //adjust focus property of the most upper element
   {
      Node *n = NULL;

      n = eina_list_last_data_get(pd->focus_stack);

      if (n)
        {
           if (!pd->redirect && old_manager)
             efl_ui_focus_object_focus_set(n->focusable, EINA_TRUE);
           else if (pd->redirect && !old_manager)
             efl_ui_focus_object_focus_set(n->focusable, EINA_FALSE);
        }
   }

   efl_event_callback_call(obj, EFL_UI_FOCUS_MANAGER_EVENT_REDIRECT_CHANGED , old_manager);

   //just set the root of the new redirect as focused, so it is in a known state
   if (redirect)
     {
        efl_ui_focus_manager_setup_on_first_touch(redirect, EFL_UI_FOCUS_DIRECTION_LAST, NULL);
     }
}

EOLIAN static Efl_Ui_Focus_Manager *
_efl_ui_focus_manager_calc_efl_ui_focus_manager_redirect_get(Eo *obj EINA_UNUSED, Efl_Ui_Focus_Manager_Calc_Data *pd)
{
   return pd->redirect;
}

static void
_free_node(void *data)
{
   Node *node = data;
   FOCUS_DATA(node->manager);

   efl_event_callback_array_del(node->focusable, focusable_node(), node->manager);

   if (pd->root != data)
     {
        node_item_free(node);
     }
}

EOLIAN static Efl_Object *
_efl_ui_focus_manager_calc_efl_object_constructor(Eo *obj, Efl_Ui_Focus_Manager_Calc_Data *pd)
{
   obj = efl_constructor(efl_super(obj, MY_CLASS));
   pd->node_hash = eina_hash_pointer_new(_free_node);
   return obj;
}

EOLIAN static Efl_Object *
_efl_ui_focus_manager_calc_efl_object_provider_find(Eo *obj, Efl_Ui_Focus_Manager_Calc_Data *pd EINA_UNUSED, const Efl_Object *klass)
{
   if (klass == MY_CLASS)
     return obj;

   return efl_provider_find(efl_super(obj, MY_CLASS), klass);
}

EOLIAN static void
_efl_ui_focus_manager_calc_efl_object_destructor(Eo *obj, Efl_Ui_Focus_Manager_Calc_Data *pd)
{
   eina_list_free(pd->focus_stack);
   eina_list_free(pd->dirty);

   eina_hash_free(pd->node_hash);

   efl_ui_focus_manager_redirect_set(obj, NULL);

   if (pd->root)
     node_item_free(pd->root);
   pd->root = NULL;

   efl_destructor(efl_super(obj, MY_CLASS));
}

typedef struct {
   Eina_Iterator iterator;
   Eina_Iterator *real_iterator;
   Efl_Ui_Focus_Manager *object;
} Border_Elements_Iterator;

static Eina_Bool
_iterator_next(Border_Elements_Iterator *it, void **data)
{
   Node *node;

   EINA_ITERATOR_FOREACH(it->real_iterator, node)
     {
        for(int i = EFL_UI_FOCUS_DIRECTION_UP ;i < EFL_UI_FOCUS_DIRECTION_LAST; i++)
          {
             if (node->type != NODE_TYPE_ONLY_LOGICAL &&
                 !DIRECTION_ACCESS(node, i).partners)
               {
                  *data = node->focusable;
                  return EINA_TRUE;
               }
          }
     }
   return EINA_FALSE;
}

static Eo *
_iterator_get_container(Border_Elements_Iterator *it)
{
   return it->object;
}

static void
_iterator_free(Border_Elements_Iterator *it)
{
   eina_iterator_free(it->real_iterator);
   free(it);
}

EOLIAN static Eina_Iterator*
_efl_ui_focus_manager_calc_efl_ui_focus_manager_border_elements_get(Eo *obj, Efl_Ui_Focus_Manager_Calc_Data *pd)
{
   Border_Elements_Iterator *it;

   dirty_flush_all(obj, pd);

   it = calloc(1, sizeof(Border_Elements_Iterator));

   EINA_MAGIC_SET(&it->iterator, EINA_MAGIC_ITERATOR);

   it->real_iterator = eina_hash_iterator_data_new(pd->node_hash);
   it->iterator.version = EINA_ITERATOR_VERSION;
   it->iterator.next = FUNC_ITERATOR_NEXT(_iterator_next);
   it->iterator.get_container = FUNC_ITERATOR_GET_CONTAINER(_iterator_get_container);
   it->iterator.free = FUNC_ITERATOR_FREE(_iterator_free);
   it->object = obj;

   return (Eina_Iterator*) it;
}

static Node*
_no_history_element(Eina_Hash *node_hash)
{
   //nothing is selected yet, just try to use the first element in the iterator
   Eina_Iterator *iter;
   Node *upper;

   iter = eina_hash_iterator_data_new(node_hash);

   EINA_ITERATOR_FOREACH(iter, upper)
     {
        if (upper->type == NODE_TYPE_NORMAL)
          break;
     }

   eina_iterator_free(iter);

   if (upper->type != NODE_TYPE_NORMAL)
     return NULL;

   return upper;
}

static void
_get_middle(Evas_Object *obj, Eina_Vector2 *elem)
{
   Eina_Rect geom;

   geom = efl_ui_focus_object_focus_geometry_get(obj);
   elem->x = geom.x + geom.w/2;
   elem->y = geom.y + geom.h/2;
}

static Node*
_coords_movement(Efl_Ui_Focus_Manager_Calc_Data *pd, Node *upper, Efl_Ui_Focus_Direction direction)
{
   Node *candidate;
   Eina_List *node_list;
   Eina_List *lst;

   EINA_SAFETY_ON_FALSE_RETURN_VAL(DIRECTION_IS_2D(direction), NULL);

   //decide which direction we take
   lst = DIRECTION_ACCESS(upper, direction).partners;
   if (!lst)
     lst = DIRECTION_ACCESS(upper, direction).one_direction;

   //we are searching which of the partners is lower to the history
   EINA_LIST_REVERSE_FOREACH(pd->focus_stack, node_list, candidate)
     {
        //we only calculate partners for normal nodes
        if (candidate->type == NODE_TYPE_NORMAL) continue;

        if (eina_list_data_find(lst, candidate))
          {
             //this is the next accessible part
             return candidate;
          }
     }

   //if we haven't found anything in the history, use the widget with the smallest distance
   {
      Eina_List *n;
      Node *node, *min = NULL;
      Eina_Vector2 elem, other;
      float min_distance = 0.0;

      _get_middle(upper->focusable, &elem);

      EINA_LIST_FOREACH(lst, n, node)
        {
           _get_middle(node->focusable, &other);
           float tmp = eina_vector2_distance_get(&other, &elem);
           if (!min || tmp < min_distance)
             {
                min = node;
                min_distance = tmp;
             }
        }
      candidate = min;
   }
   return candidate;
}


static Node*
_prev_item(Node *node)
{
   Node *parent;
   Eina_List *lnode;

   parent = T(node).parent;

   //we are accessing the parents children, prepare!
   efl_ui_focus_object_prepare_logical(parent->focusable);

   lnode = eina_list_data_find_list(T(parent).children, node);
   lnode = eina_list_prev(lnode);

   if (lnode)
     return eina_list_data_get(lnode);
   return NULL;
}

static Node*
_next(Node *node)
{
   Node *n;

   //Case 1 we are having children
   //But only enter the children if it does NOT have a redirect manager
   if (T(node).children && !node->redirect_manager)
     {
        return eina_list_data_get(T(node).children);
     }

   //case 2 we are the root and we don't have children, return ourself
   if (!T(node).parent)
     {
        return node;
     }

   //case 3 we are not at the end of the parents list
   n = node;
   while(T(n).parent)
     {
        Node *parent;
        Eina_List *lnode;

        parent = T(n).parent;

        //we are accessing the parents children, prepare!
        efl_ui_focus_object_prepare_logical(parent->focusable);

        lnode = eina_list_data_find_list(T(parent).children, n);
        lnode = eina_list_next(lnode);

        if (lnode)
          {
             return eina_list_data_get(lnode);
          }

        n = parent;
     }

   //this is then the root again
   return NULL;
}

static Node*
_prev(Node *node)
{
   Node *n = NULL;

   //this is the root there is no parent
   if (!T(node).parent)
     return NULL;

   n =_prev_item(node);

   //we are accessing prev items children, prepare them!
   if (n && n->focusable)
     efl_ui_focus_object_prepare_logical(n->focusable);

   //case 1 there is a item in the parent previous to node, which has children
   if (n && T(n).children && !n->redirect_manager)
     {
        do
          {
              n = eina_list_last_data_get(T(n).children);
          }
        while (T(n).children && !n->redirect_manager);

        return n;
     }

   //case 2 there is a item in the parent previous to node, which has no children
   if (n)
     return n;

   //case 3 there is a no item in the parent previous to this one
   return T(node).parent;
}


static Node*
_logical_movement(Efl_Ui_Focus_Manager_Calc_Data *pd EINA_UNUSED, Node *upper, Efl_Ui_Focus_Direction direction)
{
   Node* (*deliver)(Node *n);
   Node *result;
   Eina_List *stack = NULL;

   EINA_SAFETY_ON_FALSE_RETURN_VAL(DIRECTION_IS_LOGICAL(direction), NULL);

   if (direction == EFL_UI_FOCUS_DIRECTION_NEXT)
     deliver = _next;
   else
     deliver = _prev;

   //search as long as we have a none logical parent
   result = upper;
   do
     {
        //give up, if we have already been here
        if (!!eina_list_data_find(stack, result))
          {
             eina_list_free(stack);
             ERR("Warning cycle detected\n");
             return NULL;
          }

        stack = eina_list_append(stack, result);

        if (direction == EFL_UI_FOCUS_DIRECTION_NEXT)
          efl_ui_focus_object_prepare_logical(result->focusable);

        result = deliver(result);
   } while(result && result->type != NODE_TYPE_NORMAL && !result->redirect_manager);

   eina_list_free(stack);

   return result;
}

static Efl_Ui_Focus_Object*
_request_move(Eo *obj EINA_UNUSED, Efl_Ui_Focus_Manager_Calc_Data *pd, Efl_Ui_Focus_Direction direction, Node *upper)
{
   Node *dir = NULL;

   if (!upper)
     upper = _focus_stack_last_regular(pd);

   if (!upper)
     {
        upper = _no_history_element(pd->node_hash);
        if (upper)
          return upper->focusable;
        return NULL;

     }

   dirty_flush(obj, pd, upper);

   if (direction == EFL_UI_FOCUS_DIRECTION_PREVIOUS
    || direction == EFL_UI_FOCUS_DIRECTION_NEXT)
      dir = _logical_movement(pd, upper, direction);
   else
      dir = _coords_movement(pd, upper, direction);

   //return the widget
   if (dir)
     return dir->focusable;
   else
     return NULL;
}

EOLIAN static Efl_Ui_Focus_Object*
_efl_ui_focus_manager_calc_efl_ui_focus_manager_request_move(Eo *obj EINA_UNUSED, Efl_Ui_Focus_Manager_Calc_Data *pd, Efl_Ui_Focus_Direction direction)
{
   EINA_SAFETY_ON_FALSE_RETURN_VAL(DIRECTION_CHECK(direction), NULL);

   if (pd->redirect)
     return efl_ui_focus_manager_request_move(pd->redirect, direction);
   else
     {
        Node *upper = NULL;

        upper = eina_list_last_data_get(pd->focus_stack);

        if (!upper)
          {
             upper = _no_history_element(pd->node_hash);
             if (upper)
               return upper->focusable;
             return NULL;
          }

        return _request_move(obj, pd, direction, upper);
     }
}

static Node*
_request_subchild(Node *node)
{
   //important! if there are no children _next would return the parent of node which will exceed the limit of children of node
   Node *target = NULL;

   if (node->tree.children)
     {
        target = node;

        //try to find a child that is not logical or has a redirect manager
        while (target && target->type == NODE_TYPE_ONLY_LOGICAL && !target->redirect_manager)
          {
             if (target != node)
               efl_ui_focus_object_prepare_logical(target->focusable);

             target = _next(target);
             //abort if we are exceeding the childrens of node
             if (target == node) target = NULL;
          }

        F_DBG("Found node %p", target);
     }

   return target;
}

EOLIAN static void
_efl_ui_focus_manager_calc_efl_ui_focus_manager_manager_focus_set(Eo *obj, Efl_Ui_Focus_Manager_Calc_Data *pd, Efl_Ui_Focus_Object *focus)
{
   Node *node, *last;
   Efl_Ui_Focus_Object *last_focusable = NULL;
   Efl_Ui_Focus_Manager *redirect_manager;

   EINA_SAFETY_ON_NULL_RETURN(focus);

   //check if node is part of this manager object
   node = node_get(obj, pd, focus);
   if (!node) return;

   if (node->type == NODE_TYPE_ONLY_LOGICAL && !node->redirect_manager)
     {
        Node *target = NULL;

        F_DBG(" %p is logical, fetching the next subnode that is either a redirect or a regular", obj);

        //important! if there are no children _next would return the parent of node which will exceed the limit of children of node
        efl_ui_focus_object_prepare_logical(node->focusable);

        target = _request_subchild(node);

        //check if we have found anything
        if (target)
          {
             node = target;
          }
        else
          {
             ERR("Could not fetch a node located at %p", node->focusable);
             return;
          }
     }

   F_DBG("Manager: %p focusing object %p %s", obj, node->focusable, efl_class_name_get(node->focusable));

   //make sure this manager is in the chain of redirects
   _manager_in_chain_set(obj, pd);

   if (eina_list_last_data_get(pd->focus_stack) == node)
     {
        //the correct one is focused
        if (node->redirect_manager == pd->redirect)
          return;
     }

   if (pd->redirect)
     {
        Efl_Ui_Focus_Manager *m = obj;

        //completely unset the current redirect chain
        while (efl_ui_focus_manager_redirect_get(m))
         {
            Efl_Ui_Focus_Manager *old = m;

            m = efl_ui_focus_manager_redirect_get(m);
            efl_ui_focus_manager_redirect_set(old, NULL);
          }
     }

   redirect_manager = node->redirect_manager;

   last = eina_list_last_data_get(pd->focus_stack);
   if (last)
     last_focusable = last->focusable;

   //remove the object from the list and add it again
   pd->focus_stack = eina_list_remove(pd->focus_stack, node);
   pd->focus_stack = eina_list_append(pd->focus_stack, node);

   /*
     Only emit those signals if we are already at the top of the focus stack.
     Otherwise focus_get in the callback to that signal might return false.
    */
   if (node->type == NODE_TYPE_NORMAL)
     {
        //populate the new change
        efl_ui_focus_object_focus_set(last_focusable, EINA_FALSE);
        efl_ui_focus_object_focus_set(node->focusable, EINA_TRUE);
        efl_event_callback_call(obj, EFL_UI_FOCUS_MANAGER_EVENT_FOCUSED, last_focusable);
     }

   //set to NULL here, from the event earlier this pointer could be dead.
   node = NULL;

   //now check if this is also a listener object
   if (redirect_manager)
     {
        //set the redirect
        efl_ui_focus_manager_redirect_set(obj, redirect_manager);
     }
}

EOLIAN static void
_efl_ui_focus_manager_calc_efl_ui_focus_manager_setup_on_first_touch(Eo *obj, Efl_Ui_Focus_Manager_Calc_Data *pd, Efl_Ui_Focus_Direction direction, Efl_Ui_Focus_Object *entry)
{
   if (direction == EFL_UI_FOCUS_DIRECTION_PREVIOUS && entry)
     {
        Efl_Ui_Focus_Manager_Logical_End_Detail last;
        Efl_Ui_Focus_Manager *rec_manager = obj;
        do
          {
             last = efl_ui_focus_manager_logical_end(rec_manager);
             EINA_SAFETY_ON_NULL_RETURN(last.element);
             efl_ui_focus_manager_focus_set(rec_manager, last.element);

             rec_manager = efl_ui_focus_manager_redirect_get(rec_manager);
          }
        while (rec_manager);
     }
   else if (DIRECTION_IS_2D(direction) && entry)
     efl_ui_focus_manager_focus_set(obj, entry);
   else
     efl_ui_focus_manager_focus_set(obj, pd->root->focusable);
}


EOLIAN static Efl_Ui_Focus_Object*
_efl_ui_focus_manager_calc_efl_ui_focus_manager_move(Eo *obj EINA_UNUSED, Efl_Ui_Focus_Manager_Calc_Data *pd, Efl_Ui_Focus_Direction direction)
{
   Efl_Ui_Focus_Object *candidate = NULL;
   Efl_Ui_Focus_Manager *early, *late;

   EINA_SAFETY_ON_FALSE_RETURN_VAL(DIRECTION_CHECK(direction), NULL);

   early = efl_ui_focus_manager_redirect_get(obj);

   if (pd->redirect)
     {
        Efl_Ui_Focus_Object *old_candidate = NULL;
        candidate = efl_ui_focus_manager_move(pd->redirect, direction);

        if (!candidate)
          {
             Efl_Ui_Focus_Object *new_candidate = NULL;

             if (DIRECTION_IS_LOGICAL(direction))
               {
                  // lets just take the last

                  Node *n = eina_list_last_data_get(pd->focus_stack);
                  new_candidate = _request_move(obj, pd, direction, n);

                  if (new_candidate)
                    efl_ui_focus_manager_focus_set(obj, new_candidate);

                  candidate = new_candidate;
               }
             else
               {
                  Node *n;

                  old_candidate = efl_ui_focus_manager_focus_get(pd->redirect);
                  n = eina_hash_find(pd->node_hash, &old_candidate);

                  if (n)
                    new_candidate = _request_move(obj, pd, direction, n);

                  if (new_candidate)
                    {
                       //redirect does not have smth. but we do have.
                       efl_ui_focus_manager_focus_set(obj, new_candidate);
                    }

                  candidate = new_candidate;
               }
          }
     }
   else
     {
        candidate = efl_ui_focus_manager_request_move(obj, direction);

        F_DBG("Manager: %p moved to %p %s in direction %d", obj, candidate, efl_class_name_get(candidate), direction);

        if (candidate)
          {
             efl_ui_focus_manager_focus_set(obj, candidate);
          }
     }

   late = efl_ui_focus_manager_redirect_get(obj);

   if (early != late)
     {
        //this is a new manager, we have to init its case!
        efl_ui_focus_manager_setup_on_first_touch(pd->redirect, direction, candidate);
     }

   return candidate;
}

EOLIAN static Eina_Bool
_efl_ui_focus_manager_calc_efl_ui_focus_manager_root_set(Eo *obj EINA_UNUSED, Efl_Ui_Focus_Manager_Calc_Data *pd, Efl_Ui_Focus_Object *root)
{
   Node *node;

   if (pd->root)
     {
        ERR("Root element can only be set once!");
        return EINA_FALSE;
     }

   node = _register(obj, pd, root, NULL);
   node->type = NODE_TYPE_ONLY_LOGICAL;

   pd->root = node;

   return EINA_TRUE;
}

EOLIAN static Efl_Ui_Focus_Object*
_efl_ui_focus_manager_calc_efl_ui_focus_manager_root_get(Eo *obj EINA_UNUSED, Efl_Ui_Focus_Manager_Calc_Data *pd)
{
   if (!pd->root) return NULL;

   return pd->root->focusable;
}

EOLIAN static Efl_Object*
_efl_ui_focus_manager_calc_efl_object_finalize(Eo *obj, Efl_Ui_Focus_Manager_Calc_Data *pd)
{
   Efl_Object *result;

   if (!pd->root)
     {
        ERR("Constructing failed. No root element set.");
        return NULL;
     }

   result = efl_finalize(efl_super(obj, MY_CLASS));

   return result;
}

static Eina_List*
_convert(Border b)
{
   Eina_List *n, *par = NULL;
   Node *node;

   EINA_LIST_FOREACH(b.partners, n, node)
     par = eina_list_append(par, node->focusable);

   EINA_LIST_FOREACH(b.one_direction, n, node)
     par = eina_list_append(par, node->focusable);

   return par;
}

EOLIAN static Efl_Ui_Focus_Object*
_efl_ui_focus_manager_calc_efl_ui_focus_manager_manager_focus_get(Eo *obj EINA_UNUSED, Efl_Ui_Focus_Manager_Calc_Data *pd)
{
   Node *upper = NULL;

   upper = eina_list_last_data_get(pd->focus_stack);

   if (!upper)
     return NULL;
   return upper->focusable;
}

EOLIAN static Efl_Ui_Focus_Relations*
_efl_ui_focus_manager_calc_efl_ui_focus_manager_fetch(Eo *obj, Efl_Ui_Focus_Manager_Calc_Data *pd, Efl_Ui_Focus_Object *child)
{
   Efl_Ui_Focus_Relations *res;
   Node *n, *tmp;

   n = node_get(obj, pd, child);
   if (!n)
      return NULL;

   res = calloc(1, sizeof(Efl_Ui_Focus_Relations));

   dirty_flush(obj, pd, n);

   //make sure to prepare_logical so next and prev are correctly
   if (n->tree.parent)
     efl_ui_focus_object_prepare_logical(n->tree.parent->focusable);
   efl_ui_focus_object_prepare_logical(n->focusable);

#define DIR_CLONE(dir) _convert(DIRECTION_ACCESS(n,dir));

   res->right = DIR_CLONE(EFL_UI_FOCUS_DIRECTION_RIGHT);
   res->left = DIR_CLONE(EFL_UI_FOCUS_DIRECTION_LEFT);
   res->top = DIR_CLONE(EFL_UI_FOCUS_DIRECTION_UP);
   res->down = DIR_CLONE(EFL_UI_FOCUS_DIRECTION_DOWN);
   res->next = (tmp = _next(n)) ? tmp->focusable : NULL;
   res->prev = (tmp = _prev(n)) ? tmp->focusable : NULL;
   res->position_in_history = eina_list_data_idx(pd->focus_stack, n);
   res->node = child;

   res->logical = (n->type == NODE_TYPE_ONLY_LOGICAL);

   if (T(n).parent)
     res->parent = T(n).parent->focusable;
   res->redirect = n->redirect_manager;
#undef DIR_CLONE

   return res;
}

EOLIAN static void
_efl_ui_focus_manager_calc_class_constructor(Efl_Class *c EINA_UNUSED)
{
   _focus_log_domain = eina_log_domain_register("elementary-focus", EINA_COLOR_CYAN);
}

EOLIAN static void
_efl_ui_focus_manager_calc_class_destructor(Efl_Class *c EINA_UNUSED)
{
   eina_log_domain_unregister(_focus_log_domain);
   _focus_log_domain = -1;
}

EOLIAN static Efl_Ui_Focus_Manager_Logical_End_Detail
_efl_ui_focus_manager_calc_efl_ui_focus_manager_logical_end(Eo *obj EINA_UNUSED, Efl_Ui_Focus_Manager_Calc_Data *pd)
{
   Node *child = pd->root;
   Efl_Ui_Focus_Manager_Logical_End_Detail ret = { 0, NULL};
   EINA_SAFETY_ON_NULL_RETURN_VAL(child, ret);

   //we need to return the most lower right element

   while ((child) && (T(child).children) && (!child->redirect_manager))
     child = eina_list_last_data_get(T(child).children);
   while ((child) && (child->type != NODE_TYPE_NORMAL) && (!child->redirect_manager))
     child = _prev(child);

   if (child)
     {
        ret.is_regular_end = child->type == NODE_TYPE_NORMAL;
        ret.element = child->focusable;
     }
   return ret;
}

EOLIAN static void
_efl_ui_focus_manager_calc_efl_ui_focus_manager_reset_history(Eo *obj EINA_UNUSED, Efl_Ui_Focus_Manager_Calc_Data *pd)
{
  Efl_Ui_Focus_Object *last_focusable;

  if (!pd->focus_stack) return;

  last_focusable = _focus_stack_unfocus_last(pd);

  pd->focus_stack = eina_list_free(pd->focus_stack);

  efl_event_callback_call(obj, EFL_UI_FOCUS_MANAGER_EVENT_FOCUSED, last_focusable);
}

EOLIAN static void
_efl_ui_focus_manager_calc_efl_ui_focus_manager_pop_history_stack(Eo *obj EINA_UNUSED, Efl_Ui_Focus_Manager_Calc_Data *pd)
{
  Efl_Ui_Focus_Object *last_focusable;
  Node *last;

  if (!pd->focus_stack) return;

  last_focusable = _focus_stack_unfocus_last(pd);

  //get now the highest, and unfocus that!
  last = eina_list_last_data_get(pd->focus_stack);
  if (last) efl_ui_focus_object_focus_set(last->focusable, EINA_TRUE);
  efl_event_callback_call(obj, EFL_UI_FOCUS_MANAGER_EVENT_FOCUSED, last_focusable);
}

EOLIAN static Efl_Ui_Focus_Object*
_efl_ui_focus_manager_calc_efl_ui_focus_manager_request_subchild(Eo *obj, Efl_Ui_Focus_Manager_Calc_Data *pd, Efl_Ui_Focus_Object *child_obj)
{
   Node *child, *target;

   child = node_get(obj, pd, child_obj);
   target = _request_subchild(child);

   if (target) return target->focusable;
   return NULL;
}

EOLIAN static void
_efl_ui_focus_manager_calc_efl_object_dbg_info_get(Eo *obj, Efl_Ui_Focus_Manager_Calc_Data *pd, Efl_Dbg_Info *root)
{
   efl_dbg_info_get(efl_super(obj, MY_CLASS), root);
   Efl_Dbg_Info *append, *group = EFL_DBG_INFO_LIST_APPEND(root, "Efl.Ui.Focus.Manager");
   Eina_Iterator *iter;
   Node *node;

   append = EFL_DBG_INFO_LIST_APPEND(group, "children");

   iter = eina_hash_iterator_data_new(pd->node_hash);
   EINA_ITERATOR_FOREACH(iter, node)
     {
        EFL_DBG_INFO_APPEND(append, "-", EINA_VALUE_TYPE_UINT64, node->focusable);
     }
   eina_iterator_free(iter);
}

#define EFL_UI_FOCUS_MANAGER_CALC_EXTRA_OPS \
   EFL_OBJECT_OP_FUNC(efl_dbg_info_get, _efl_ui_focus_manager_calc_efl_object_dbg_info_get)

#include "efl_ui_focus_manager_calc.eo.c"
