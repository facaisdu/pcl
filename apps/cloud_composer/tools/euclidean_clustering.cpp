#include <pcl/apps/cloud_composer/tools/euclidean_clustering.h>
#include <pcl/apps/cloud_composer/items/cloud_item.h>

#include <pcl/point_types.h>
#include <pcl/filters/extract_indices.h>
#include <pcl/kdtree/kdtree.h>
#include <pcl/segmentation/extract_clusters.h>

Q_EXPORT_PLUGIN2(cloud_composer_euclidean_clustering_tool, pcl::cloud_composer::EuclideanClusteringToolFactory)

pcl::cloud_composer::EuclideanClusteringTool::EuclideanClusteringTool (PropertiesModel* parameter_model, QObject* parent)
  : SplitItemTool (parameter_model, parent)
{
  
}

pcl::cloud_composer::EuclideanClusteringTool::~EuclideanClusteringTool ()
{
  
}

QList <pcl::cloud_composer::CloudComposerItem*>
pcl::cloud_composer::EuclideanClusteringTool::performAction (ConstItemList input_data)
{
  QList <CloudComposerItem*> output;
  const CloudComposerItem* input_item;
  // Check input data length
  if ( input_data.size () == 0)
  {
    qCritical () << "Empty input in Euclidean Clustering Tool!";
    return output;
  }
  else if ( input_data.size () > 1)
  {
    qWarning () << "Input vector has more than one item in Euclidean Clustering!";
  }
  input_item = input_data.value (0);
  
  sensor_msgs::PointCloud2::ConstPtr input_cloud;
  if (input_item->getCloudConstPtr (input_cloud))
  {
    double cluster_tolerance = parameter_model_->getProperty ("Cluster Tolerance").toDouble();
    int min_cluster_size = parameter_model_->getProperty ("Min Cluster Size").toInt();
    int max_cluster_size = parameter_model_->getProperty ("Max Cluster Size").toInt();
   

    //Get the cloud in template form
    pcl::PointCloud<pcl::PointXYZ>::Ptr cloud (new pcl::PointCloud<pcl::PointXYZ>);
    pcl::fromROSMsg (*input_cloud, *cloud); 
     
    //////////////// THE WORK - COMPUTING CLUSTERS ///////////////////
    // Creating the KdTree object for the search method of the extraction
    pcl::search::KdTree<pcl::PointXYZ>::Ptr tree (new pcl::search::KdTree<pcl::PointXYZ>);
    tree->setInputCloud (cloud);
  
    std::vector<pcl::PointIndices> cluster_indices;
    pcl::EuclideanClusterExtraction<pcl::PointXYZ> ec;
    ec.setClusterTolerance (cluster_tolerance); 
    ec.setMinClusterSize (min_cluster_size);
    ec.setMaxClusterSize (max_cluster_size);
    ec.setSearchMethod (tree);
    ec.setInputCloud (cloud);
    ec.extract (cluster_indices);
    //////////////////////////////////////////////////////////////////
    //Get copies of the original origin and orientation
    Eigen::Vector4f source_origin = input_item->data (ORIGIN).value<Eigen::Vector4f> ();
    Eigen::Quaternionf source_orientation =  input_item->data (ORIENTATION).value<Eigen::Quaternionf> ();
    //Vector to accumulate the extracted indices
    pcl::IndicesPtr extracted_indices (new std::vector<int> ());
    //Put found clusters into new cloud_items!
    qDebug () << "Found "<<cluster_indices.size ()<<" clusters!";
    int cluster_count = 0;
    pcl::ExtractIndices<sensor_msgs::PointCloud2> filter;
    for (std::vector<pcl::PointIndices>::const_iterator it = cluster_indices.begin (); it != cluster_indices.end (); ++it)
    {
      filter.setInputCloud (input_cloud);
      // It's annoying that I have to do this, but Euclidean returns a PointIndices struct
      pcl::IndicesPtr indices_to_extract (new std::vector<int> (it->indices));
      filter.setIndices (indices_to_extract);
      extracted_indices->insert (extracted_indices->end (), it->indices.begin (), it->indices.end ());
      //This means remove the other points
      filter.setKeepOrganized (false);
      sensor_msgs::PointCloud2::Ptr cloud_filtered (new sensor_msgs::PointCloud2);
      filter.filter (*cloud_filtered);
      qDebug() << "Cluster has " << cloud_filtered->width << " data points.";
      CloudItem* cloud_item = new CloudItem (input_item->text ()+tr("-Clstr %1").arg(cluster_count)
                                             , cloud_filtered
                                             , source_origin
                                             , source_orientation);
      output.append (cloud_item);
      ++cluster_count;
    } 
    //Now make a cloud containing all the remaining points
    filter.setIndices (extracted_indices);
    filter.setNegative (true);
    sensor_msgs::PointCloud2::Ptr remainder_cloud (new sensor_msgs::PointCloud2);
    filter.filter (*remainder_cloud);
    qDebug() << "Cloud has " << remainder_cloud->width << " data points after clusters removed.";
    CloudItem* cloud_item = new CloudItem (input_item->text ().arg(cluster_count)
                                             , remainder_cloud
                                             , source_origin
                                             , source_orientation);
    output.push_front (cloud_item);
  }
  else
  {
    qCritical () << "Input item in Clustering is not a cloud!!!";
  }
  
  
  return output;
}

/////////////////// PARAMETER MODEL /////////////////////////////////
pcl::cloud_composer::PropertiesModel*
pcl::cloud_composer::EuclideanClusteringToolFactory::createToolParameterModel (QObject* parent)
{
  PropertiesModel* parameter_model = new PropertiesModel(parent);
  
  parameter_model->addProperty ("Cluster Tolerance", 0.02,  Qt::ItemIsEditable | Qt::ItemIsEnabled);
  parameter_model->addProperty ("Min Cluster Size", 100,  Qt::ItemIsEditable | Qt::ItemIsEnabled);
  parameter_model->addProperty ("Max Cluster Size", 25000,  Qt::ItemIsEditable | Qt::ItemIsEnabled);
    

  return parameter_model;
}