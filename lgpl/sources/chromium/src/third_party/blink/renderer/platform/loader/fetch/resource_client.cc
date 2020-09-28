/*
    Copyright (C) 1998 Lars Knoll (knoll@mpi-hd.mpg.de)
    Copyright (C) 2001 Dirk Mueller <mueller@kde.org>
    Copyright (C) 2004, 2005, 2006, 2007, 2008, 2009, 2010, 2011 Apple Inc. All
    rights reserved.

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Library General Public License
    along with this library; see the file COPYING.LIB.  If not, write to
    the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
    Boston, MA 02110-1301, USA.

    This class provides all functionality needed for loading images, style
    sheets and html pages from the web. It has a memory cache for these objects.
*/

#include "third_party/blink/renderer/platform/loader/fetch/resource_client.h"

#include "third_party/blink/renderer/platform/loader/fetch/resource.h"

namespace blink {

void ResourceClient::Trace(Visitor* visitor) const {
  visitor->Trace(resource_);
}

void ResourceClient::SetResource(Resource* new_resource,
                                 base::SingleThreadTaskRunner* task_runner) {
  if (new_resource == resource_)
    return;

  // Some ResourceClient implementations reenter this so we need to
  // prevent double removal.
  if (Resource* old_resource = resource_.Release())
    old_resource->RemoveClient(this);
  resource_ = new_resource;
  if (resource_)
    resource_->AddClient(this, task_runner);
}

}  //  namespace blink
