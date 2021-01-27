
#ifndef __CONTAINER_H
#define __CONTAINER_H

#include <assert.h>

template <typename T> class container {
private:
	struct entity_t {
		size_t referencecount;
		T value;
		entity_t()
			: referencecount(0)
		{
		}
	};
	entity_t *entity;
	void assign(entity_t *p)
	{
		if (p) {
			p->referencecount++;
		}
		if (entity) {
			if (entity->referencecount > 1) {
				entity->referencecount--;
			} else {
				delete entity;
			}
		}
		entity = p;
	}
public:
	container()
		: entity(0)
	{
	}
	container(container const &r)
		: entity(0)
	{
		assign(r.entity);
	}
	virtual ~container()
	{
		assign(0);
	}
	void operator = (container const &r)
	{
		assign(r.entity);
	}
	T *operator -> ()
	{
		if (!entity) {
			assign(new entity_t());
		}
		return &entity->value;
	}
	T const *operator -> () const
	{
		return entity ? &entity->value : 0;
	}
	bool has() const
	{
		return entity != 0;
	}
	container &create()
	{
		if (!entity) {
			assign(new entity_t());
		}
		return *this;
	}
	T &operator * ()
	{
		if (!entity) {
			assign(new entity_t());
		}
		return entity->value;
	}
	T const &operator * () const
	{
		assert(entity);
		return entity->value;
	}
	bool operator == (container const &r) const
	{
		if (entity && r.entity) {
			return entity->value == r.entity->value;
		}
		return !entity && !r.entity;
	}
	bool operator != (container const &r) const
	{
		if (entity && r.entity) {
			return entity->value != r.entity->value;
		}
		return entity || r.entity;
	}
	bool operator < (container const &r) const
	{
		if (entity && r.entity) {
			return entity->value < r.entity->value;
		}
		if (r.entity) {
			return true;
		}
		return false;
	}
	bool operator > (container const &r) const
	{
		if (entity && r.entity) {
			return entity->value > r.entity->value;
		}
		if (entity) {
			return true;
		}
		return false;
	}
	bool operator <= (container const &r) const
	{
		if (entity && r.entity) {
			return entity->value <= r.entity->value;
		}
		if (!entity) {
			return false;
		}
		return false;
	}
	bool operator >= (container const &r) const
	{
		if (entity && r.entity) {
			return entity->value >= r.entity->value;
		}
		if (!r.entity) {
			return false;
		}
		return false;
	}
};

#endif
