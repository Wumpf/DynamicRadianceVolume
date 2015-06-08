using UnityEngine;
using UnityEditor;
using System.Collections.Generic;
using System.IO;
using System.Text;
using System.Runtime.InteropServices;
using System.Runtime.Serialization;

static class Extensions
{
    public static Vector4 ToV4(this Vector3 v, float w)
    {
        return new Vector4(v.x, v.y, v.z, w);
    }
}

class Exporter
{
    [StructLayout(LayoutKind.Sequential, Pack = 1)]
    private struct Vertex
    {
        public Vector3 position;
        public Vector3 normal;
        public Vector4 tangent; // The 4th component determines the handedness of the bitangent
        public Vector2 texcoord;
    };

    const int rawModelVersion = 2;

    private List<Vertex> vertexBuffer = new List<Vertex>();
    private List<int> indexBuffer = new List<int>();
    private HashSet<Texture> encounteredTextures = new HashSet<Texture>();

    public void Export(GameObject[] gameObjects, string jsonFilename, bool copyTextures)
    {
        if (gameObjects.Length == 0)
        {
            Debug.Log("Did not export any meshes! Nothing was selected!");
            return;
        }

        Bounds boundingBox = new Bounds();
        boundingBox.SetMinMax(new Vector3(float.MaxValue, float.MaxValue, float.MaxValue), new Vector3(float.MinValue, float.MinValue, float.MinValue));

        string rawFilename = ChangeFileEndingTo(jsonFilename, "rawbuffer");

        JSONObject jsonHeader = new JSONObject(JSONObject.Type.OBJECT);
        jsonHeader.AddField("version", rawModelVersion);
        jsonHeader.AddField("originalFilename", EditorApplication.currentScene);
        jsonHeader.AddField("rawbufferFilename", rawFilename);

        rawFilename = Path.Combine(Path.GetDirectoryName(jsonFilename), rawFilename);

        JSONObject jsonMeshes = new JSONObject(JSONObject.Type.ARRAY);

        Stack<GameObject> objectStack = new Stack<GameObject>(Selection.gameObjects);
        while (objectStack.Count > 0)
        {
            GameObject gameObject = objectStack.Pop();
            if(!gameObject.activeInHierarchy)
                continue;
            for (int child = 0; child < gameObject.transform.childCount; ++child)
                objectStack.Push(gameObject.transform.GetChild(child).gameObject);

            Renderer renderer = gameObject.GetComponent<Renderer>();
            if (renderer != null)
            {
                List<int> startIndices = new List<int>();

                // Vertices & triangles
                var transformMatrix = renderer.localToWorldMatrix;

                var meshFilters = gameObject.GetComponents<MeshFilter>();
                foreach (var meshFilter in meshFilters)
                {
                    Mesh mesh = meshFilter.sharedMesh;

                    // Check mesh topologies
                    bool invalidTopology = false;
                    for (int submesh = 0; submesh < mesh.subMeshCount; ++submesh)
                    {
                        if (mesh.GetTopology(submesh) != MeshTopology.Triangles)
                        {
                            Debug.LogError("Can not export mesh " + gameObject.name + "/" + mesh.name + " since the topology of at least one submesh is unsuppoted (!= TRIANGLES!).");
                            invalidTopology = true;
                        }
                    }
                    if (invalidTopology)
                        continue;


                    int indexOffset = vertexBuffer.Count;
                    vertexBuffer.Capacity += mesh.vertexCount;

                    var positions = mesh.vertices;
                    var normals = mesh.normals;
                    var tangents = mesh.tangents;
                    var texcoords = mesh.uv;
                    for (int v = 0; v < mesh.vertexCount; ++v)
                    {
                        vertexBuffer.Add(new Vertex()
                        {
                            position = transformMatrix.MultiplyPoint(positions[v]),
                            normal = transformMatrix.MultiplyVector(normals[v]),
                            tangent = transformMatrix.MultiplyVector(new Vector3(tangents[v].x, tangents[v].y, tangents[v].z)).ToV4(tangents[v].w),
                            texcoord = new Vector2(texcoords[v].x, 1.0f - texcoords[v].y) // Convert to OpenGL convention
                        });
                    }

                    for (int submesh = 0; submesh < mesh.subMeshCount; ++submesh)
                    {
                        startIndices.Add(indexBuffer.Count);

                        var triangles = mesh.GetTriangles(submesh);
                        indexBuffer.Capacity += triangles.Length;

                        for (int i = 0; i < triangles.Length; ++i)
                        {
                            indexBuffer.Add(triangles[i] + indexOffset);
                        }
                    }
                }

                boundingBox.Encapsulate(renderer.bounds);

                var materials = renderer.sharedMaterials;
                for (int mesh = 0; mesh < startIndices.Count; ++mesh)
                {
                    JSONObject jsonMesh = new JSONObject();
                    jsonMesh.AddField("startIndex", startIndices[mesh]);
                    if (mesh + 1 < startIndices.Count)
                        jsonMesh.AddField("numIndices", startIndices[mesh + 1] - startIndices[mesh]);
                    else
                        jsonMesh.AddField("numIndices", indexBuffer.Count - startIndices[mesh]);

                    if (materials.Length > mesh)
                    {
                        ExtractMaterial(materials[mesh], ref jsonMesh);
                    }
                    else
                    {
                        jsonMesh.AddField("alphaTesting", false);
                        jsonMesh.AddField("diffuseOrigin", new JSONObject(new JSONObject[] { new JSONObject(1.0f), new JSONObject(1.0f), new JSONObject(1.0f) }));
                        jsonMesh.AddField("normalmapOrigin", "*default*");
                        jsonMesh.AddField("roughnessOrigin", 0.4f);
                        jsonMesh.AddField("metallicOrigin", 0.001f);
                    }

                    jsonMeshes.Add(jsonMesh);
                }
            }
        }


        jsonHeader.AddField("numVertices", vertexBuffer.Count);
        jsonHeader.AddField("numTriangles", indexBuffer.Count / 3);

        // Bounding box
        JSONObject jsonBoundingBox = new JSONObject();
        jsonBoundingBox.AddField("min", new JSONObject(new JSONObject[] { new JSONObject(boundingBox.min.x), new JSONObject(boundingBox.min.y), new JSONObject(boundingBox.min.z) }));
        jsonBoundingBox.AddField("max", new JSONObject(new JSONObject[] { new JSONObject(boundingBox.max.x), new JSONObject(boundingBox.max.y), new JSONObject(boundingBox.max.z) }));

        // Combine everything
        JSONObject jsonRoot = new JSONObject();
        jsonRoot.AddField("header", jsonHeader);
        jsonRoot.AddField("boundingBox", jsonBoundingBox);
        jsonRoot.AddField("meshes", jsonMeshes);

        // Write rawbuffer
        Debug.Log("Writing raw buffer...");
        using (FileStream file = new FileStream(rawFilename, FileMode.Create))
        {
            using (var writer = new BinaryWriter(file))
            {
                for (int i = 0; i < vertexBuffer.Count; ++i)
                {
                    writer.Write(vertexBuffer[i].position.x);
                    writer.Write(vertexBuffer[i].position.y);
                    writer.Write(vertexBuffer[i].position.z);
                    writer.Write(vertexBuffer[i].normal.x);
                    writer.Write(vertexBuffer[i].normal.y);
                    writer.Write(vertexBuffer[i].normal.z);
                    writer.Write(vertexBuffer[i].tangent.x);
                    writer.Write(vertexBuffer[i].tangent.y);
                    writer.Write(vertexBuffer[i].tangent.z);
                    writer.Write(vertexBuffer[i].tangent.w);
                    writer.Write(vertexBuffer[i].texcoord.x);
                    writer.Write(vertexBuffer[i].texcoord.y);
                }

                for (int i = 0; i < indexBuffer.Count; ++i)
                {
                    writer.Write(indexBuffer[i]);
                }
            }
        }

        // Write JSON
        Debug.Log("Writing json buffer...");
        File.WriteAllText(jsonFilename, jsonRoot.ToString());

        // Write Textures...
        if (copyTextures)
        {
            Debug.Log("Copying Textures...");
            string directory = Path.GetDirectoryName(jsonFilename);
            foreach(Texture texture in encounteredTextures)
            {
                /*Texture2D texture2D = texture as Texture2D;
                if(texture2D == null)
                {
                    Debug.LogError("Texture " + texture.name + " is not a 2D texture. Can't copy & convert.");
                    continue;
                }*/

                string origin = AssetDatabase.GetAssetPath(texture.GetInstanceID());
                string destination = Path.Combine(directory, AssetDatabase.GetAssetPath(texture.GetInstanceID()));

                try
                {
                    Directory.CreateDirectory(Path.GetDirectoryName(destination));
                    File.Copy(origin, destination, true);
                }
                catch(System.Exception e)
                {
                    Debug.LogError("Failed to copy texture " + origin + ": " + e.Message);
                }
            }
        }

        Debug.Log("Export done!");
    }

    private static string ChangeFileEndingTo(string filename, string newExtension)
    {
        return Path.GetFileNameWithoutExtension(filename) + "." + newExtension;
    }


    // Overview over standard shader property names.
    // http://forum.unity3d.com/threads/access-rendering-mode-var-on-standard-shader-via-scripting.287002/

    private void ExtractMaterial(Material material, ref JSONObject jsonMesh)
    {
        if(material.HasProperty("_Mode") && material.GetFloat("_Mode") == 1.0f)
            jsonMesh.AddField("alphaTesting", true);
        else
            jsonMesh.AddField("alphaTesting", false);

        Texture texture = null;

        if (material.HasProperty("_MainTex"))
            texture = material.GetTexture("_MainTex");
        if (texture != null)
        {
            encounteredTextures.Add(texture);
          
            string filename = AssetDatabase.GetAssetPath(texture.GetInstanceID());
            jsonMesh.AddField("diffuseOrigin", filename);
        }
        else if (material.HasProperty("_Color"))
        {
            Color diffuse = material.GetColor("_Color");
            jsonMesh.AddField("diffuseOrigin", new JSONObject(new JSONObject[] { new JSONObject(diffuse.r), new JSONObject(diffuse.g), new JSONObject(diffuse.b) }));
        }
        else
            jsonMesh.AddField("diffuseOrigin", new JSONObject(new JSONObject[] { new JSONObject(1.0f), new JSONObject(1.0f), new JSONObject(1.0f) }));


        texture = null;
        if (material.HasProperty("_BumpMap"))
            texture = material.GetTexture("_BumpMap");
        if (texture != null)
        {
            encounteredTextures.Add(texture);

            string filename = AssetDatabase.GetAssetPath(texture.GetInstanceID());

            jsonMesh.AddField("normalmapOrigin", filename);
        }
        else
            jsonMesh.AddField("normalmapOrigin", "*default*");


        texture = null;
        if (material.HasProperty("_MetallicGlossMap"))
            texture = material.GetTexture("_MetallicGlossMap");
        if (texture != null)
        {
            encounteredTextures.Add(texture);

            string filename = AssetDatabase.GetAssetPath(texture.GetInstanceID());

            JSONObject roughnessOrigin = new JSONObject();
            roughnessOrigin.AddField("filename", filename);
            roughnessOrigin.AddField("channel", "a");
            roughnessOrigin.AddField("inverted", true);
            jsonMesh.AddField("roughnessOrigin", roughnessOrigin);

            JSONObject metallicOrigin = new JSONObject();
            metallicOrigin.AddField("filename", filename);
            metallicOrigin.AddField("channel", "r");
            jsonMesh.AddField("metallicOrigin", metallicOrigin);
        }
        else if (material.HasProperty("_Metallic") && material.HasProperty("_Glossiness"))
        {
            jsonMesh.AddField("roughnessOrigin", 1.0f - material.GetFloat("_Glossiness"));
            jsonMesh.AddField("metallicOrigin", material.GetFloat("_Metallic"));
        }
        else
        {
            jsonMesh.AddField("roughnessOrigin", 0.4f);
            jsonMesh.AddField("metallicOrigin", 0.001f);
        }
    }
}

public class RawModelExport
{
    [MenuItem("Export/Export RawModel")]
    static void Export()
    {
        Exporter exporter = new Exporter();
        exporter.Export(Selection.gameObjects, EditorUtility.SaveFilePanel("Export .json file", "", Path.GetFileNameWithoutExtension(EditorApplication.currentScene) + ".json", "json"), false);
    }

    [MenuItem("Export/Export RawModel, Copy Textures")]
    static void ExportWithTextures()
    {
        Exporter exporter = new Exporter();
        exporter.Export(Selection.gameObjects, EditorUtility.SaveFilePanel("Export .json file", "", Path.GetFileNameWithoutExtension(EditorApplication.currentScene) + ".json", "json"), true);
    }
}