/****************************************************************************
 Freeciv - Copyright (C) 2004 - The Freeciv Team
   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.
****************************************************************************/

#ifdef HAVE_CONFIG_H
#include <fc_config.h>
#endif

/* utility */
#include "log.h"

/* common */
#include "city.h"
#include "effects.h"
#include "game.h"
#include "map.h"
#include "tile.h"

#include "traderoutes.h"

const char *trade_route_type_names[] = {
  "National", "NationalIC", "IN", "INIC"
};

const char *traderoute_cancelling_type_names[] = {
  "Active", "Inactive", "Cancel"
};

struct trade_route_settings trtss[TRT_LAST];

/*************************************************************************
  Return current maximum number of trade routes city can have.
*************************************************************************/
int max_trade_routes(const struct city *pcity)
{
  int eft = get_city_bonus(pcity, EFT_MAX_TRADE_ROUTES);

  return CLIP(0, eft, MAX_TRADE_ROUTES);
}

/*************************************************************************
  What is type of the traderoute between two cities.
*************************************************************************/
enum trade_route_type cities_trade_route_type(const struct city *pcity1,
                                              const struct city *pcity2)
{
  if (city_owner(pcity1) != city_owner(pcity2)) {
    if (city_tile(pcity1)->continent != city_tile(pcity2)->continent) {
      return TRT_IN_IC;
    } else {
      return TRT_IN;
    }
  } else {
    if (city_tile(pcity1)->continent != city_tile(pcity2)->continent) {
      return TRT_NATIONAL_IC;
    } else {
      return TRT_NATIONAL;
    }
  }

  return TRT_LAST;
}

/*************************************************************************
  Return percentage bonus for trade route type.
*************************************************************************/
int trade_route_type_trade_pct(enum trade_route_type type)
{
  if (type < 0 || type >= TRT_LAST) {
    return 0;
  }

  return trtss[type].trade_pct;
}

/*************************************************************************
  Initialize trade route types.
*************************************************************************/
void trade_route_types_init(void)
{
  enum trade_route_type type;

  for (type = TRT_NATIONAL; type < TRT_LAST; type++) {
    struct trade_route_settings *set = trade_route_settings_by_type(type);

    set->trade_pct = 100;
  }
}

/*************************************************************************
  Return human readable name of trade route type
*************************************************************************/
const char *trade_route_type_name(enum trade_route_type type)
{
  fc_assert_ret_val(type >= TRT_NATIONAL && type < TRT_LAST, NULL);

  return trade_route_type_names[type];
}

/*************************************************************************
  Get trade route type by name.
*************************************************************************/
enum trade_route_type trade_route_type_by_name(const char *name)
{
  enum trade_route_type type;

  for (type = TRT_NATIONAL; type < TRT_LAST; type++) {
    if (!fc_strcasecmp(trade_route_type_names[type], name)) {
      return type;
    }
  }

  return TRT_LAST;
}

/*************************************************************************
  Return human readable name of traderoute cancelling type
*************************************************************************/
const char *traderoute_cancelling_type_name(enum traderoute_illegal_cancelling type)
{
  fc_assert_ret_val(type >= TRI_ACTIVE && type < TRI_LAST, NULL);

  return traderoute_cancelling_type_names[type];
}

/*************************************************************************
  Get traderoute cancelling type by name.
*************************************************************************/
enum traderoute_illegal_cancelling traderoute_cancelling_type_by_name(const char *name)
{
  enum traderoute_illegal_cancelling type;

  for (type = TRI_ACTIVE; type < TRI_LAST; type++) {
    if (!fc_strcasecmp(traderoute_cancelling_type_names[type], name)) {
      return type;
    }
  }

  return TRI_LAST;
}

/*************************************************************************
  Get trade route settings related to type.
*************************************************************************/
struct trade_route_settings *
trade_route_settings_by_type(enum trade_route_type type)
{
  fc_assert_ret_val(type >= TRT_NATIONAL && type < TRT_LAST, NULL);

  return &trtss[type];
}

/**************************************************************************
  Return TRUE iff the two cities are capable of trade; i.e., if a caravan
  from one city can enter the other to sell its goods.

  See also can_establish_trade_route().
**************************************************************************/
bool can_cities_trade(const struct city *pc1, const struct city *pc2)
{
  /* If you change the logic here, make sure to update the help in
   * helptext_unit(). */
  return (pc1 && pc2 && pc1 != pc2
          && (city_owner(pc1) != city_owner(pc2)
              || map_distance(pc1->tile, pc2->tile)
                 >= game.info.trademindist)
          && (trade_route_type_trade_pct(cities_trade_route_type(pc1, pc2))
              > 0));
}

/****************************************************************************
  Return the minimum value of the sum of trade routes which could be
  replaced by a new one. The target cities of the concerned trade routes
  are will be put into 'would_remove', if set.
****************************************************************************/
int city_trade_removable(const struct city *pcity,
                         struct city_list *would_remove)
{
  int sorted[MAX_TRADE_ROUTES];
  int num, i, j;

  FC_STATIC_ASSERT(ARRAY_SIZE(sorted) == ARRAY_SIZE(pcity->trade),
                   incompatible_trade_array_size);
  FC_STATIC_ASSERT(ARRAY_SIZE(sorted) == ARRAY_SIZE(pcity->trade_value),
                   incompatible_trade_value_array_size);

  /* Sort trade route values. */
  num = 0;
  for (i = 0; i < MAX_TRADE_ROUTES; i++) {
    if (0 == pcity->trade[i]) {
      continue;
    }
    for (j = num; j > 0 && (pcity->trade_value[i]
                            < pcity->trade_value[sorted[j - 1]]); j--) {
      sorted[j] = sorted[j - 1];
    }
    sorted[j] = i;
    num++;
  }

  /* No trade routes at all. */
  if (0 == num) {
    return 0;
  }

  /* Adjust number of concerned trade routes. */
  num += 1 - max_trade_routes(pcity);
  if (0 >= num) {
    num = 1;
  }

  /* Return values. */
  for (i = j = 0; i < num; i++) {
    j += pcity->trade_value[sorted[i]];
    if (NULL != would_remove) {
      struct city *pother = game_city_by_number(pcity->trade[sorted[i]]);

      fc_assert(NULL != pother);
      city_list_append(would_remove, pother);
    }
  }
  return j;
}

/**************************************************************************
  Returns TRUE iff the two cities can establish a trade route.  We look
  at the distance and ownership of the cities as well as their existing
  trade routes.  Should only be called if you already know that
  can_cities_trade().
**************************************************************************/
bool can_establish_trade_route(const struct city *pc1, const struct city *pc2)
{
  int trade = -1;
  int maxpc1;
  int maxpc2;

  if (!pc1 || !pc2 || pc1 == pc2
      || !can_cities_trade(pc1, pc2)
      || have_cities_trade_route(pc1, pc2)) {
    return FALSE;
  }

  /* First check if cities can have trade routes at all. */
  maxpc1 = max_trade_routes(pc1);
  if (maxpc1 <= 0) {
    return FALSE;
  }
  maxpc2 = max_trade_routes(pc2);
  if (maxpc2 <= 0) {
    return FALSE;
  }
    
  if (city_num_trade_routes(pc1) >= maxpc1) {
    trade = trade_between_cities(pc1, pc2);
    /* can we replace trade route? */
    if (city_trade_removable(pc1, NULL) >= trade) {
      return FALSE;
    }
  }
  
  if (city_num_trade_routes(pc2) >= maxpc2) {
    if (trade == -1) {
      trade = trade_between_cities(pc1, pc2);
    }
    /* can we replace trade route? */
    if (city_trade_removable(pc2, NULL) >= trade) {
      return FALSE;
    }
  }  

  return TRUE;
}

/**************************************************************************
  Return the trade that exists between these cities, assuming they have a
  trade route.
**************************************************************************/
int trade_between_cities(const struct city *pc1, const struct city *pc2)
{
  int bonus = 0;

  if (NULL != pc1 && NULL != pc1->tile
      && NULL != pc2 && NULL != pc2->tile) {

    bonus = real_map_distance(pc1->tile, pc2->tile)
            + city_size_get(pc1) + city_size_get(pc2);

    bonus = bonus * trade_route_type_trade_pct(cities_trade_route_type(pc1, pc2)) / 100;

    bonus /= 12;
  }

  return bonus;
}

/**************************************************************************
 Return number of trade route city has
**************************************************************************/
int city_num_trade_routes(const struct city *pcity)
{
  int i, n = 0;

  for (i = 0; i < MAX_TRADE_ROUTES; i++) {
    if (pcity->trade[i] != 0) {
      n++;
    }
  }
  
  return n;
}

/*********************************************************************//**
  Comparator used in max_tile_trade.
**************************************************************************/
static int best_value(const void *a, const void *b)
{
  return *(int *)a < *(int *)b;
}

 /*********************************************************************//**
  Returns the maximum trade production of the tiles of the city.
**************************************************************************/
static int max_tile_trade(const struct city *pcity)
{
  int i, total = 0;
  int radius_sq = city_map_radius_sq_get(pcity);
  int tile_trade[city_map_tiles(radius_sq)];
  size_t size = 0;
  bool is_celebrating = base_city_celebrating(pcity);
   if (pcity->tile == NULL) {
    return 0;
  }
   city_map_iterate(radius_sq, cindex, cx, cy) {
    struct tile *ptile = city_map_to_tile(pcity->tile, radius_sq, cx, cy);
     if (ptile == NULL) {
      continue;
    }
     if (is_free_worked_index(cindex)) {
      total += city_tile_output(pcity, ptile, is_celebrating, O_TRADE);
      continue;
    }
     if (!city_can_work_tile(pcity, ptile)) {
      continue;
    }
     tile_trade[size++] = city_tile_output(pcity, ptile, is_celebrating,
                                          O_TRADE);
  } city_map_iterate_end;
   qsort(tile_trade, size, sizeof(*tile_trade), best_value);
   for (i = 0; i < pcity->size && i < size; i++) {
    total += tile_trade[i];
  }
   return total;
}

/*********************************************************************//**
  Returns the maximum trade production of a city.
**************************************************************************/
static int max_trade_prod(const struct city *pcity)
{
  /* Trade tile base */
  int trade_prod = max_tile_trade(pcity);
   /* Add trade routes values */
  trade_routes_iterate(pcity, partner) {
    trade_prod += trade_between_cities(pcity, partner);
  } trade_routes_iterate_end;
   return trade_prod;
}

/**************************************************************************
  Returns the revenue trade bonus - you get this when establishing a
  trade route and also when you simply sell your trade goods at the
  new city.

  Note if you trade with a city you already have a trade route with,
  you'll only get 1/3 of this value.
**************************************************************************/
int get_caravan_enter_city_trade_bonus(const struct city *pc1, 
                                       const struct city *pc2)
{
  int tb = 0, bonus = 0;

  if (game.server.caravan_bonus_style == CBS_CLASSIC) {
    /* Should this be real_map_distance? */
    tb = map_distance(pc1->tile, pc2->tile) + 10;
    tb = (tb * (pc1->surplus[O_TRADE] + pc2->surplus[O_TRADE])) / 24;
  } else if (game.server.caravan_bonus_style == CBS_LOGARITHMIC) {
    /* Logarithmic bonus */
    bonus = pow(log(real_map_distance(pc1->tile, pc2->tile) + 20
                + max_trade_prod(pc1) + max_trade_prod(pc2)) * 2, 2);
    tb = (int)bonus;
  }

  /* Trade_revenue_bonus increases revenue by power of 2 in milimes */
  bonus = get_city_bonus(pc1, EFT_TRADE_REVENUE_BONUS);
  
  tb = (float)tb * pow(2.0, (double)bonus / 1000.0);

  return tb;
}

/**************************************************************************
  Check if cities have an established trade route.
**************************************************************************/
bool have_cities_trade_route(const struct city *pc1, const struct city *pc2)
{
  trade_routes_iterate(pc1, route_to) {
    if (route_to->id == pc2->id) {
      return TRUE;
    }
  } trade_routes_iterate_end;

  return FALSE;
}
