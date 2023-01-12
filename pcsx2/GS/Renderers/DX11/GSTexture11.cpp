/*
 *	Copyright (C) 2007-2009 Gabest
 *	http://www.gabest.org
 *
 *  This Program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2, or (at your option)
 *  any later version.
 *
 *  This Program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with GNU Make; see the file COPYING.  If not, write to
 *  the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA USA.
 *  http://www.gnu.org/copyleft/gpl.html
 *
 */

#include "Pcsx2Types.h"

#include "GSTexture11.h"

GSTexture11::GSTexture11(ID3D11Texture2D* texture)
	: m_texture(texture), m_layer(0)
{
	m_texture->GetDevice(&m_dev);
	m_texture->GetDesc(&m_desc);

	m_dev->GetImmediateContext(&m_ctx);

	m_size.x = (int)m_desc.Width;
	m_size.y = (int)m_desc.Height;

	if(m_desc.BindFlags & D3D11_BIND_RENDER_TARGET) m_type = RenderTarget;
	else if(m_desc.BindFlags & D3D11_BIND_DEPTH_STENCIL) m_type = DepthStencil;
	else if(m_desc.BindFlags & D3D11_BIND_SHADER_RESOURCE) m_type = Texture;
	else if(m_desc.Usage == D3D11_USAGE_STAGING) m_type = Offscreen;

	m_format = (int)m_desc.Format;

	m_max_layer = m_desc.MipLevels;
}

bool GSTexture11::Update(const GSVector4i& r, const void* data, int pitch, int layer)
{
	if(layer >= m_max_layer)
		return true;

	if(m_dev && m_texture)
	{
		D3D11_BOX box = { (UINT)r.left, (UINT)r.top, 0U, (UINT)r.right, (UINT)r.bottom, 1U };
		UINT subresource = layer; // MipSlice + (ArraySlice * MipLevels).

		m_ctx->UpdateSubresource(m_texture, subresource, &box, data, pitch, 0);

		return true;
	}

	return false;
}

bool GSTexture11::Map(GSMap& m, const GSVector4i* r, int layer)
{
	if(r != NULL)
		return false;

	if(layer >= m_max_layer)
		return false;

	if(m_texture && m_desc.Usage == D3D11_USAGE_STAGING)
	{
		D3D11_MAPPED_SUBRESOURCE map;
		UINT subresource = layer;

		if(SUCCEEDED(m_ctx->Map(m_texture, subresource, D3D11_MAP_READ_WRITE, 0, &map)))
		{
			m.bits  = (u8*)map.pData;
			m.pitch = (int)map.RowPitch;

			m_layer = layer;

			return true;
		}
	}

	return false;
}

void GSTexture11::Unmap()
{
	if(m_texture)
	{
		UINT subresource = m_layer;
		m_ctx->Unmap(m_texture, subresource);
	}
}

GSTexture11::operator ID3D11Texture2D*()
{
	return m_texture;
}

GSTexture11::operator ID3D11ShaderResourceView*()
{
	if(!m_srv && m_dev && m_texture)
	{
		if(m_desc.Format == DXGI_FORMAT_R32G8X24_TYPELESS)
		{
			D3D11_SHADER_RESOURCE_VIEW_DESC srvd = {};

			srvd.Format = DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS;
			srvd.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
			srvd.Texture2D.MipLevels = 1;

			m_dev->CreateShaderResourceView(m_texture, &srvd, &m_srv);
		}
		else
		{
			m_dev->CreateShaderResourceView(m_texture, NULL, &m_srv);
		}
	}

	return m_srv;
}

GSTexture11::operator ID3D11RenderTargetView*()
{
	if(!m_rtv && m_dev && m_texture)
		m_dev->CreateRenderTargetView(m_texture, NULL, &m_rtv);

	return m_rtv;
}

GSTexture11::operator ID3D11DepthStencilView*()
{
	if(!m_dsv && m_dev && m_texture)
	{
		if(m_desc.Format == DXGI_FORMAT_R32G8X24_TYPELESS)
		{
			D3D11_DEPTH_STENCIL_VIEW_DESC dsvd = {};

			dsvd.Format = DXGI_FORMAT_D32_FLOAT_S8X24_UINT;
			dsvd.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;

			m_dev->CreateDepthStencilView(m_texture, &dsvd, &m_dsv);
		}
		else
		{
			m_dev->CreateDepthStencilView(m_texture, NULL, &m_dsv);
		}
	}

	return m_dsv;
}

bool GSTexture11::Equal(GSTexture11* tex)
{
	return tex && m_texture == tex->m_texture;
}
